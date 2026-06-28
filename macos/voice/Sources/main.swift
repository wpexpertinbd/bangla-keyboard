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
import Speech
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
            let out = Punct.apply(t, bangla: false)
            DispatchQueue.main.async { AppDelegate.inject(out) }
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

    /// Gate on permissions FIRST (so we never call AVAudioEngine without mic access — that
    /// throws an uncatchable ObjC exception and crashes). Re-entrant: async grants call back in.
    func setMode(_ m: Int) {
        if mode == m { stopAll(); mode = 0; renderIcon(); return }   // toggle off

        // 1) Microphone (required for both).
        switch AVCaptureDevice.authorizationStatus(for: .audio) {
        case .authorized: break
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .audio) { ok in
                DispatchQueue.main.async { ok ? self.setMode(m) : self.permAlert("Microphone") }
            }
            return
        default:
            permAlert("Microphone"); return
        }

        // 2) Speech Recognition (English on-device path).
        if m == 2 {
            switch SFSpeechRecognizer.authorizationStatus() {
            case .authorized, .denied, .restricted: break      // denied -> HTTP en-US fallback
            case .notDetermined:
                SpeechEN.authorize { _ in DispatchQueue.main.async { self.setMode(2) } }
                return
            @unknown default: break
            }
        }

        beginMode(m)
    }

    private func beginMode(_ m: Int) {
        stopAll()
        mode = m; bnFail = 0; bnLang = "bn-BD"
        do {
            if m == 1 { try audio.start() }                        // Bangla -> Google bn-BD
            else if speechEN.available { try speechEN.start() }    // English -> on-device
            else { try audio.start() }                             // English fallback -> Google en-US
        } catch {
            mode = 0; renderIcon()
            notify("Couldn’t start the microphone",
                   "Make sure no other app is using the mic, and that Microphone access is enabled in System Settings → Privacy & Security.")
            return
        }
        renderIcon()
        // Typing into other apps needs Accessibility — warn once if it's missing.
        if !AXIsProcessTrusted() { axWarnOnce() }
    }

    private func permAlert(_ what: String) {
        mode = 0; renderIcon()
        let a = NSAlert()
        a.messageText = "\(what) access is off"
        a.informativeText = "Enable \(what) for “Bangla Voice” in System Settings → Privacy & Security → \(what), then try again."
        a.addButton(withTitle: "Open Settings"); a.addButton(withTitle: "Cancel")
        if a.runModal() == .alertFirstButtonReturn {
            let leaf = what == "Microphone" ? "Privacy_Microphone" : "Privacy_SpeechRecognition"
            if let u = URL(string: "x-apple.systempreferences:com.apple.preference.security?\(leaf)") { NSWorkspace.shared.open(u) }
        }
    }

    private var axWarned = false
    private func axWarnOnce() {
        if axWarned { return }
        axWarned = true
        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(opts)   // shows the system "grant Accessibility" prompt
    }

    private func stopAll() { audio.stop(); speechEN.stop() }

    // MARK: recognition
    private func handleUtterance(_ pcm: Data) {
        let lang = (mode == 1) ? bnLang : "en-US"
        let isBn = (mode == 1)
        sttQueue.async { [weak self] in
            guard let self else { return }
            let r = STT.recognize(pcm: pcm, lang: lang)
            let t = r.text.trimmingCharacters(in: .whitespacesAndNewlines)
            if t.isEmpty {
                // Fall back to bn-IN only on a REAL HTTP error (network/4xx/5xx), NOT on
                // silence (a clean 2xx with no transcript) — else a brief pause flips Bangla
                // to the weaker recognizer. Require ~3 real errors.
                if isBn && r.httpError {
                    self.bnFail += 1
                    if self.bnFail >= 3 && self.bnLang == "bn-BD" { self.bnLang = "bn-IN" }
                }
                return
            }
            if isBn { self.bnFail = 0 }
            let out = Punct.apply(t, bangla: isBn)
            DispatchQueue.main.async { AppDelegate.inject(out) }
        }
    }

    /// Inject text, first backspacing the previous utterance's trailing space when this one
    /// starts with a standalone punctuation mark (so "…আছি" + "।" -> "…আছি।").
    static func inject(_ out: String) {
        if Punct.leadsWithMark(out) { Inject.backspace() }
        Inject.text(out)
    }

    // MARK: misc
    @objc private func grantAX() {
        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(opts)
    }
    @objc private func about() {
        let a = NSAlert()
        a.messageText = "Bangla Voice"
        a.informativeText = "Free voice typing for the Bangla Keyboard.\n⌃⌥S Bangla · ⌃⌥D English.\n\nSay punctuation as a separate word (after a pause): দাঁড়ি → ।, কমা → ,, প্রশ্ন → ?  ·  English: “full stop”, “comma”, “question mark”.\n\nNothing is stored; mic is live only while listening.\n\nBiswasHost · MIT"
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
