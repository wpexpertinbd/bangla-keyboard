// Bangla Keyboard — Voice (Bangla Voice.app)
//
// Free online voice typing companion for the macOS keyboard layout. A menu-bar agent:
//   ⌃⌥S = Bangla voice,  ⌃⌥D = English voice  (press again, or the menu, to stop).
// Speak -> recognized text is inserted at the cursor in any app. Bangla uses Google's
// free bn-BD endpoint (correct Bangladeshi Bangla; see STT.swift); on repeated throttle
// it falls back to bn-IN. English uses en-US. A "।" / "." is auto-appended per utterance;
// English is capitalized. The menu-bar mic shows state: blue = listening, green = speaking.
//
// Privacy: the mic is live ONLY while a mode is on; no audio or text is ever stored or
// logged; the only network call is the speech request. Opt-in (you launch the app).
//
// Needs: Microphone permission (capture) + Accessibility permission (to type into apps).
import AppKit
import AVFoundation
import Carbon.HIToolbox

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusItem: NSStatusItem!
    private let audio = AudioCapture()
    private let speechEN = SpeechEN()
    private var mode = 0                 // 0 off, 1 Bangla, 2 English
    private var bnFail = 0
    private var bnLang = "bn-BD"
    private let sttQueue = DispatchQueue(label: "voice.stt", qos: .userInitiated)

    func applicationDidFinishLaunching(_ n: Notification) {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        buildMenu()
        renderIcon()

        audio.onState = { [weak self] talking in
            DispatchQueue.main.async { self?.renderIcon(talking: talking) }
        }
        audio.onUtterance = { [weak self] pcm in self?.handleUtterance(pcm) }

        // English = on-device SFSpeechRecognizer (reliable/live; the Google key's en-US is throttled).
        speechEN.onState = { [weak self] talking in
            DispatchQueue.main.async { if self?.mode == 2 { self?.renderIcon(talking: talking) } }
        }
        speechEN.onText = { t in
            let out = AppDelegate.finish(t, bangla: false)
            DispatchQueue.main.async { Inject.text(out) }
        }

        AVCaptureDevice.requestAccess(for: .audio) { _ in }   // prompt early
        SpeechEN.authorize { _ in }                           // prompt for speech recognition
        registerHotkeys()
    }

    // MARK: menu-bar item
    private func buildMenu() {
        let m = NSMenu()
        m.addItem(withTitle: "Bangla Voice  (⌃⌥S)", action: #selector(toggleBangla), keyEquivalent: "")
        m.addItem(withTitle: "English Voice  (⌃⌥D)", action: #selector(toggleEnglish), keyEquivalent: "")
        m.addItem(.separator())
        m.addItem(withTitle: "Grant Accessibility (to type)…", action: #selector(grantAX), keyEquivalent: "")
        m.addItem(withTitle: "About", action: #selector(about), keyEquivalent: "")
        m.addItem(.separator())
        m.addItem(withTitle: "Quit Bangla Voice", action: #selector(quit), keyEquivalent: "q")
        statusItem.menu = m
    }

    /// blue = listening (mode on, silent), green = speaking, dim = off.
    private func renderIcon(talking: Bool = false) {
        guard let btn = statusItem.button else { return }
        let on = mode != 0
        let name = on ? "mic.fill" : "mic"
        let img = NSImage(systemSymbolName: name, accessibilityDescription: "Bangla Voice")
        img?.isTemplate = false
        btn.image = img
        if !on { btn.contentTintColor = NSColor.secondaryLabelColor }
        else if talking { btn.contentTintColor = NSColor.systemGreen }
        else { btn.contentTintColor = NSColor.systemBlue }
        btn.toolTip = on ? (mode == 1 ? "Bangla voice — listening" : "English voice — listening")
                         : "Bangla Voice (idle)"
    }

    // MARK: mode
    @objc private func toggleBangla()  { setMode(1) }
    @objc private func toggleEnglish() { setMode(2) }

    func setMode(_ m: Int) {
        if mode == m {                                  // same mode -> turn off
            stopAll(); mode = 0; renderIcon(); return
        }
        stopAll()
        mode = m; bnFail = 0; bnLang = "bn-BD"
        do {
            if m == 1 {
                try audio.start()                       // Bangla -> VAD + Google bn-BD
            } else if speechEN.available {
                try speechEN.start()                    // English -> on-device recognizer
            } else {
                try audio.start()                       // English fallback -> Google en-US (throttle-prone)
            }
        } catch {
            mode = 0; renderIcon()
            notify(m == 2 ? "English voice unavailable"
                          : "Microphone unavailable",
                   m == 2 ? "Allow Speech Recognition + Microphone in System Settings → Privacy & Security, then try again."
                          : "Could not start audio capture. Allow Microphone access and try again.")
            return
        }
        renderIcon()
    }

    private func stopAll() { audio.stop(); speechEN.stop() }

    // MARK: recognition
    private func handleUtterance(_ pcm: Data) {
        let lang = (mode == 1) ? bnLang : "en-US"
        let isBn = (mode == 1)
        sttQueue.async { [weak self] in
            guard let self else { return }
            let raw = STT.recognize(pcm: pcm, lang: lang)
            let t = raw.trimmingCharacters(in: .whitespacesAndNewlines)
            if t.isEmpty {
                if isBn {                                // throttle/empty -> bn-IN fallback
                    self.bnFail += 1
                    if self.bnFail >= 2 && self.bnLang == "bn-BD" { self.bnLang = "bn-IN" }
                }
                return
            }
            if isBn { self.bnFail = 0 }
            let out = Self.finish(t, bangla: isBn)
            DispatchQueue.main.async { Inject.text(out) }
        }
    }

    /// Punctuation/capitalization rules from voice.html.
    static func finish(_ text: String, bangla: Bool) -> String {
        var t = text
        let endsPunct = t.hasSuffix("।") || t.hasSuffix(".") || t.hasSuffix("!") || t.hasSuffix("?")
        if bangla {
            if !endsPunct { t += "।" }
        } else {
            if let f = t.first { t = f.uppercased() + t.dropFirst() }
            if !endsPunct { t += "." }
        }
        return t + " "
    }

    // MARK: misc
    @objc private func grantAX() {
        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(opts)
    }
    @objc private func about() {
        let a = NSAlert()
        a.messageText = "Bangla Voice"
        a.informativeText = "Free voice typing for the Bangla Keyboard.\n⌃⌥S Bangla · ⌃⌥D English.\nNothing is stored; mic is live only while listening.\n\nBiswasHost · MIT"
        a.runModal()
    }
    @objc private func quit() { stopAll(); NSApp.terminate(nil) }
    private func notify(_ title: String, _ body: String) {
        let a = NSAlert(); a.messageText = title; a.informativeText = body; a.runModal()
    }

    // MARK: global hotkeys (Carbon — work without focus; injection still needs Accessibility)
    private func registerHotkeys() {
        var spec = EventTypeSpec(eventClass: OSType(kEventClassKeyboard),
                                 eventKind: UInt32(kEventHotKeyPressed))
        InstallEventHandler(GetApplicationEventTarget(), { _, evt, _ -> OSStatus in
            var hk = EventHotKeyID()
            GetEventParameter(evt, EventParamName(kEventParamDirectObject), EventParamType(typeEventHotKeyID),
                              nil, MemoryLayout<EventHotKeyID>.size, nil, &hk)
            DispatchQueue.main.async {
                if let d = NSApp.delegate as? AppDelegate { d.setMode(hk.id == 1 ? 1 : 2) }
            }
            return noErr
        }, 1, &spec, nil, nil)

        var ref: EventHotKeyRef?
        let mods = UInt32(controlKey | optionKey)
        RegisterEventHotKey(UInt32(kVK_ANSI_S), mods, EventHotKeyID(signature: fourCC("BkVo"), id: 1),
                            GetApplicationEventTarget(), 0, &ref)
        RegisterEventHotKey(UInt32(kVK_ANSI_D), mods, EventHotKeyID(signature: fourCC("BkVo"), id: 2),
                            GetApplicationEventTarget(), 0, &ref)
    }
}

func fourCC(_ s: String) -> OSType {
    var r: OSType = 0
    for b in s.utf8.prefix(4) { r = (r << 8) | OSType(b) }
    return r
}

let app = NSApplication.shared
app.setActivationPolicy(.accessory)             // menu-bar agent, no Dock icon
let delegate = AppDelegate()
app.delegate = delegate
app.run()
