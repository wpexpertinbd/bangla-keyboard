// Microphone capture + voice-activity detection, a native port of windows/voice/voice.html.
// AVAudioEngine taps the input; we gate on per-buffer RMS energy (> 0.012 = speech),
// collect float samples while speaking, treat > 700 ms of trailing silence as the end of
// an utterance, then linear-downsample to 16 kHz Int16 and hand the PCM to a callback.
// Audio lives only in memory for the current utterance — nothing is written or kept.
import AVFoundation

final class AudioCapture {
    private let engine = AVAudioEngine()
    private var collecting = false
    private var speaking = false
    private var silenceMs = 0.0
    private var buf: [Float] = []
    private var inRate = 48000.0

    // VAD constants — identical to voice.html.
    private let rmsGate: Float = 0.012
    private let silenceEndMs = 700.0
    private let minSamples16k = 1600          // < 0.1 s @16k -> noise, drop

    /// Called when speaking starts/stops (for the menu-bar mic colour).
    var onState: ((Bool) -> Void)?
    /// Called on each completed utterance with 16 kHz mono LINEAR16 PCM.
    var onUtterance: ((Data) -> Void)?

    var isRunning: Bool { collecting }

    func start() throws {
        guard !collecting else { return }
        let input = engine.inputNode
        let fmt = input.inputFormat(forBus: 0)
        // No mic access / no device -> invalid format. Bail with an error instead of
        // letting installTap() throw an uncatchable ObjC exception and crash.
        guard fmt.sampleRate > 0, fmt.channelCount > 0 else {
            throw NSError(domain: "AudioCapture", code: 2,
                          userInfo: [NSLocalizedDescriptionKey: "No microphone input available"])
        }
        inRate = fmt.sampleRate
        buf.removeAll(); speaking = false; silenceMs = 0

        input.installTap(onBus: 0, bufferSize: 4096, format: fmt) { [weak self] pcm, _ in
            self?.process(pcm)
        }
        engine.prepare()
        try engine.start()
        collecting = true
    }

    func stop() {
        guard collecting else { return }
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        collecting = false
        if speaking { speaking = false; onState?(false) }
        buf.removeAll(); silenceMs = 0
    }

    private func process(_ pcm: AVAudioPCMBuffer) {
        guard let ch = pcm.floatChannelData else { return }
        let n = Int(pcm.frameLength)
        if n == 0 { return }
        let x = ch[0]                                   // channel 0

        var sum: Float = 0
        for i in 0..<n { sum += x[i] * x[i] }
        let rms = (sum / Float(n)).squareRoot()

        if rms > rmsGate {                              // speech
            if !speaking { speaking = true; onState?(true) }
            silenceMs = 0
            buf.append(contentsOf: UnsafeBufferPointer(start: x, count: n))
        } else if speaking {                            // trailing silence
            buf.append(contentsOf: UnsafeBufferPointer(start: x, count: n))
            silenceMs += Double(n) / inRate * 1000.0
            if silenceMs > silenceEndMs {               // utterance end
                speaking = false; onState?(false)
                flush()
            }
        }
    }

    private func flush() {
        let frames = buf
        buf.removeAll(); silenceMs = 0
        if frames.isEmpty { return }
        let pcm16 = downsampleTo16k(frames, inRate: inRate)
        if pcm16.count < minSamples16k { return }       // < 0.1 s -> noise
        var data = Data(capacity: pcm16.count * 2)
        for s in pcm16 { var le = s.littleEndian; withUnsafeBytes(of: &le) { data.append(contentsOf: $0) } }
        onUtterance?(data)
    }

    /// Linear-resample float [-1,1] @ inRate -> Int16 @ 16 kHz (port of down16() in voice.html).
    private func downsampleTo16k(_ f: [Float], inRate: Double) -> [Int16] {
        let ratio = inRate / 16000.0
        let outLen = Int(Double(f.count) / ratio)
        guard outLen > 0 else { return [] }
        var out = [Int16](repeating: 0, count: outLen)
        for i in 0..<outLen {
            let idx = Double(i) * ratio
            let i0 = Int(idx)
            let fr = Float(idx - Double(i0))
            let a = i0 < f.count ? f[i0] : 0
            let b = (i0 + 1) < f.count ? f[i0 + 1] : 0
            let v = a * (1 - fr) + b * fr
            out[i] = Int16(max(-32768, min(32767, Int(v * 32767))))
        }
        return out
    }
}
