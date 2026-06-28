// Free Bangla/English STT via Google's legacy speech-api/v2 endpoint — the same one
// the Windows build (windows/voice/stt_http.h) and the open-source recognize_google use.
// We POST 16 kHz mono LINEAR16 (audio/l16) PCM from native code (no CORS) and read back
// the transcript. The endpoint is UNOFFICIAL + rate-limited (throttles under load), so
// callers fall back (bn-BD -> bn-IN) on repeated empty results. Nothing is stored.
import Foundation

enum STT {
    // The well-known free Chromium speech key (shared, may be throttled by Google).
    static let key = "AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw"

    // Ephemeral session: no disk cache, no cookies, no credential storage — the audio and
    // response never touch disk, so "nothing is stored" is literally true.
    private static let session: URLSession = {
        let c = URLSessionConfiguration.ephemeral
        c.urlCache = nil
        c.httpCookieStorage = nil
        c.httpShouldSetCookies = false
        c.requestCachePolicy = .reloadIgnoringLocalCacheData
        return URLSession(configuration: c)
    }()

    struct Result { let text: String; let httpError: Bool }

    /// POST raw L16 PCM (16 kHz mono) -> transcript + a REAL-error flag.
    /// `httpError` is true ONLY on a network failure or a non-2xx/3xx status (incl. throttle
    /// 4xx/5xx). A clean 2xx with no transcript means *silence*, NOT a failure — so the caller
    /// falls back to bn-IN on actual throttling, not on a quiet chunk. Call off the main thread.
    static func recognize(pcm: Data, rate: Int = 16000, lang: String) -> Result {
        var comps = URLComponents(string: "https://www.google.com/speech-api/v2/recognize")!
        comps.queryItems = [
            URLQueryItem(name: "output", value: "json"),
            URLQueryItem(name: "lang", value: lang),
            URLQueryItem(name: "key", value: key),
        ]
        guard let url = comps.url else { return Result(text: "", httpError: true) }
        var req = URLRequest(url: url, timeoutInterval: 15)
        req.httpMethod = "POST"
        req.httpShouldHandleCookies = false
        req.setValue("audio/l16; rate=\(rate)", forHTTPHeaderField: "Content-Type")
        req.setValue("BanglaVoice/1.1", forHTTPHeaderField: "User-Agent")
        req.httpBody = pcm

        var out = ""
        var httpErr = true                       // assume error until a clean 2xx/3xx
        let sem = DispatchSemaphore(value: 0)
        let task = session.dataTask(with: req) { data, resp, _ in
            defer { sem.signal() }
            guard let http = resp as? HTTPURLResponse else { return }   // network error -> httpErr stays true
            if (200..<400).contains(http.statusCode) {
                httpErr = false                  // clean response; empty just means silence
                if let data = data, let body = String(data: data, encoding: .utf8) {
                    out = firstTranscript(body)
                }
            }                                    // 4xx/5xx -> httpErr stays true (real throttle/error)
        }
        task.resume()
        sem.wait()
        return Result(text: out, httpError: httpErr)
    }

    /// The endpoint returns line-delimited JSON; pull the first "transcript":"..." value.
    /// (Mirrors extractTranscript() in stt_http.h, but JSONDecoder handles the escapes.)
    static func firstTranscript(_ body: String) -> String {
        for line in body.split(whereSeparator: { $0 == "\n" || $0 == "\r" }) {
            guard line.contains("\"transcript\"") else { continue }
            guard let data = line.data(using: .utf8),
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let results = obj["result"] as? [[String: Any]] else { continue }
            for r in results {
                if let alts = r["alternative"] as? [[String: Any]] {
                    for a in alts {
                        if let t = a["transcript"] as? String, !t.isEmpty { return t }
                    }
                }
            }
        }
        return ""
    }
}
