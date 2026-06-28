// English voice via the OS on-device recognizer (Speech.framework) — the macOS analog
// of the Windows build's Web Speech path. Reliable + live + free, and not subject to the
// shared Google key's en-US throttling (which is why the HTTP path is unreliable for the
// most-used language). We drive utterance endpointing ourselves with the same RMS/silence
// VAD as the Bangla path: on a trailing-silence boundary we endAudio() to finalize the
// segment, inject it, and start a fresh segment. Nothing is stored.
import AVFoundation
import Speech

final class SpeechEN {
    private let engine = AVAudioEngine()
    private let recognizer = SFSpeechRecognizer(locale: Locale(identifier: "en-US"))
    private var request: SFSpeechAudioBufferRecognitionRequest?
    private var task: SFSpeechRecognitionTask?
    private var running = false
    private var speaking = false
    private var finalizing = false
    private var silenceMs = 0.0
    private var inRate = 48000.0
    private var lastText = ""

    private let rmsGate: Float = 0.012
    private let silenceEndMs = 800.0

    var onState: ((Bool) -> Void)?
    var onText: ((String) -> Void)?            // a finalized English utterance
    var isRunning: Bool { running }

    /// Usable only once authorized AND the recognizer reports available (model present).
    var available: Bool {
        SFSpeechRecognizer.authorizationStatus() == .authorized && (recognizer?.isAvailable ?? false)
    }

    static func authorize(_ cb: @escaping (Bool) -> Void) {
        SFSpeechRecognizer.requestAuthorization { st in
            DispatchQueue.main.async { cb(st == .authorized) }
        }
    }

    func start() throws {
        guard !running, let rec = recognizer, rec.isAvailable else {
            throw NSError(domain: "SpeechEN", code: 1)
        }
        let input = engine.inputNode
        let fmt = input.inputFormat(forBus: 0)
        guard fmt.sampleRate > 0, fmt.channelCount > 0 else {
            throw NSError(domain: "SpeechEN", code: 2,
                          userInfo: [NSLocalizedDescriptionKey: "No microphone input available"])
        }
        speaking = false; finalizing = false; silenceMs = 0
        inRate = fmt.sampleRate
        startSegment()
        input.installTap(onBus: 0, bufferSize: 4096, format: fmt) { [weak self] buf, _ in
            self?.feed(buf)
        }
        engine.prepare()
        try engine.start()
        running = true
    }

    func stop() {
        guard running else { return }
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        task?.cancel(); task = nil; request = nil
        running = false
        if speaking { speaking = false; onState?(false) }
    }

    private func startSegment() {
        let req = SFSpeechAudioBufferRecognitionRequest()
        req.shouldReportPartialResults = true
        if recognizer?.supportsOnDeviceRecognition == true { req.requiresOnDeviceRecognition = true }
        request = req
        lastText = ""
        finalizing = false
        task = recognizer?.recognitionTask(with: req) { [weak self] result, error in
            guard let self else { return }
            if let r = result {
                self.lastText = r.bestTranscription.formattedString
                if r.isFinal { self.emit() }
            } else if error != nil {
                self.emit()                         // segment ended/errored — flush + restart
            }
        }
    }

    private func emit() {
        guard running else { return }
        let t = lastText.trimmingCharacters(in: .whitespacesAndNewlines)
        task?.cancel(); task = nil; request = nil
        if !t.isEmpty { onText?(t) }
        startSegment()                              // ready for the next utterance
    }

    private func feed(_ buf: AVAudioPCMBuffer) {
        if !finalizing { request?.append(buf) }
        guard let ch = buf.floatChannelData else { return }
        let n = Int(buf.frameLength); if n == 0 { return }
        let x = ch[0]
        var s: Float = 0; for i in 0..<n { s += x[i] * x[i] }
        let rms = (s / Float(n)).squareRoot()
        if rms > rmsGate {
            if !speaking { speaking = true; onState?(true) }
            silenceMs = 0
        } else if speaking && !finalizing {
            silenceMs += Double(n) / inRate * 1000.0
            if silenceMs > silenceEndMs {
                speaking = false; onState?(false)
                finalizing = true
                request?.endAudio()                 // -> isFinal -> emit()
            }
        }
    }
}
