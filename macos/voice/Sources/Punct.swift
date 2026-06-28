// Spoken punctuation — ported from windows/voice (voicehost.cpp applyBanglaPunct +
// voice.html applyPunct). Free online STT returns no punctuation and free auto-punctuation
// doesn't exist, so the user SPEAKS the mark (like Google Docs / Gboard voice typing).
//
// Key rule (Windows v1.1.3): a mark is produced ONLY when the WHOLE trimmed utterance is
// exactly a command word (said alone, after a pause) — exact-match, NOT a per-token replace —
// so a real word inside a sentence (দাড়ি = "beard", "I have a question") is never clobbered.
import Foundation

enum Punct {
    // Bengali command word -> mark. Includes Google's common MIS-HEARINGS (কমা→কহনা,
    // দাঁড়ি→দাড়ি/দাড়ী, প্রশ্ন→প্রসন, …) so the intended mark still lands.
    static let bn: [String: String] = [
        "দাঁড়ি": "।", "দাড়ি": "।", "দাড়ী": "।", "ফুলস্টপ": "।",
        "কমা": ",", "কহনা": ",", "কহানা": ",", "কম্মা": ",",
        "প্রশ্ন": "?", "প্রশ্নবোধক": "?", "প্রশ্নচিহ্ন": "?", "প্রসন": "?",
        "বিস্ময়": "!", "বিস্ময়বোধক": "!", "আশ্চর্যবোধক": "!",
        "সেমিকোলন": ";", "কোলন": ":", "হাইফেন": "-", "ড্যাশ": "-",
    ]
    static let en: [String: String] = [
        "comma": ",", "period": ".", "dot": ".", "question": "?", "exclamation": "!",
        "semicolon": ";", "colon": ":", "hyphen": "-", "dash": "-",
    ]
    static let enPhrase: [String: String] = [
        "full stop": ".", "question mark": "?", "exclamation mark": "!",
    ]
    static let marks: Set<Character> = ["।", ",", ".", "?", "!", ";", ":", "-"]

    /// Whole-utterance command word -> "<mark> "; otherwise the text unchanged + a space.
    static func apply(_ text: String, bangla: Bool) -> String {
        let s = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if s.isEmpty { return "" }
        if bangla {
            if let m = bn[s] { return m + " " }
        } else {
            let low = s.lowercased()
            if let m = enPhrase[low] { return m + " " }
            if let m = en[low] { return m + " " }
        }
        return s + " "
    }

    /// Does the (space-stripped) start of `s` begin with a punctuation mark? If so we
    /// backspace the trailing space from the previous utterance so the mark attaches.
    static func leadsWithMark(_ s: String) -> Bool {
        guard let first = s.first(where: { $0 != " " }) else { return false }
        return marks.contains(first)
    }
}
