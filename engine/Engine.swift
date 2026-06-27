// Bangla Keyboard — reordering engine (Windows/Bijoy-style, fixed layout).
// Buffers one syllable (consonant cluster + vowel + trailing signs) and commits
// it on a syllable boundary, so a prebase vowel typed FIRST (ে then ক) reorders
// into correct Unicode order (কে) with no character left composing at rest.
import Foundation

enum EngineResult {
    case ignore                              // not ours — host should handle the event
    case compose(String)                     // show as marked (composing) text
    case commit(String)                      // insert as final text; buffer cleared
    case commitThenCompose(String, String)   // insert final, then begin a new composing syllable
}

final class Engine {

    // MARK: buffer (current syllable)
    private var cons  = ""   // consonant cluster, e.g. "ক্ষ" (may end in ্ mid-cluster)
    private var vowel = ""   // the vowel sign (prebase or postbase, combined)
    private var trail = ""   // trailing signs: ং ঃ ঁ

    private func render() -> String { cons + vowel + trail }   // marked (composing) text
    // Final (committed) text: a vowel sign with no base consonant becomes the
    // matching INDEPENDENT vowel (isolated া -> আ, ি -> ই, ে -> এ, …).
    private func renderFinal() -> String {
        if cons.isEmpty && !vowel.isEmpty { return (Engine.indep[vowel] ?? vowel) + trail }
        return render()
    }
    private static let indep: [String: String] = [
        "া":"আ", "ি":"ই", "ী":"ঈ", "ু":"উ", "ূ":"ঊ", "ৃ":"ঋ",
        "ে":"এ", "ৈ":"ঐ", "ো":"ও", "ৌ":"ঔ"
    ]
    // grapheme-safe "ends in hasanta (্, U+09CD)" — String.hasSuffix fails here
    // because e.g. "ক্" is a single grapheme cluster, not "ক" + "্".
    private func endsWithHasanta(_ s: String) -> Bool { s.unicodeScalars.last?.value == 0x09CD }
    private var isEmpty: Bool { cons.isEmpty && vowel.isEmpty && trail.isEmpty }
    private func reset() { cons = ""; vowel = ""; trail = "" }

    /// Commit whatever is composing (used on Cmd/Ctrl, focus loss, space, etc.).
    func flush() -> String { let s = renderFinal(); reset(); return s }

    // MARK: categories
    private static let prebase: Set<String> = ["ি", "ে", "ৈ"]
    private static let postbase: Set<String> = ["া", "ী", "ু", "ূ", "ৃ", "ৗ"]
    private static let trailSigns: Set<String> = ["ং", "ঃ", "ঁ"]
    private static let independents: Set<String> = ["অ", "আ", "ই", "ঈ", "উ", "ঊ", "ঋ", "এ", "ঐ", "ও", "ঔ"]
    private static let standalone: Set<String> = ["ৎ"]   // khanda-ta: never takes a vowel
    private static let hasanta = "্"
    // reph (র্) and folas (্র ্য) attach to the consonant cluster
    private static let clusterMods: Set<String> = ["র্", "্র", "্য"]

    private func isConsonant(_ u: String) -> Bool {
        // a single Bengali consonant letter (U+0995…U+09B9 + ড় ঢ় য়), not a sign
        guard u.count == 1, let s = u.unicodeScalars.first else { return false }
        let v = s.value
        return (0x0995...0x09B9).contains(v) || v == 0x09DC || v == 0x09DD || v == 0x09DF
    }

    /// Combine prebase + postbase into the proper single matra where needed.
    private func combineVowel(_ existing: String, _ add: String) -> String {
        switch (existing, add) {
        case ("ে", "া"): return "ো"   // e-kar + aa  -> o-kar
        case ("ে", "ৗ"): return "ৌ"   // e-kar + au-length -> au-kar
        default: return existing + add
        }
    }

    // MARK: main
    func process(keyCode: UInt16, shift: Bool, characters: String) -> EngineResult {
        guard let unit = Engine.map[Key(keyCode, shift)] else {
            // Unmapped (function keys, arrows, etc.). Backspace edits the buffer.
            if keyCode == 51, !isEmpty {           // delete/backspace while composing
                popLast()
                return isEmpty ? .commit("") : .compose(render())
            }
            return .ignore
        }

        // Hasanta
        if unit == Engine.hasanta {
            cons += unit
            return .compose(render())
        }
        // Reph / folas — extend the cluster
        if Engine.clusterMods.contains(unit) {
            cons += unit
            return .compose(render())
        }
        // Prebase vowel (ি ে ৈ) — always typed BEFORE its consonant (Windows/Bijoy).
        // So if anything is already buffered, that syllable is done; this vowel begins
        // a NEW syllable and waits for the next consonant (হ then ে -> commit হ, then চ্ছে).
        if Engine.prebase.contains(unit) {
            if !isEmpty {
                let out = renderFinal(); reset(); vowel = unit
                return .commitThenCompose(out, render())
            }
            vowel = unit
            return .compose(render())
        }
        // Postbase matra (া ী ু ূ ৃ ৗ)
        if Engine.postbase.contains(unit) {
            if cons.isEmpty && vowel.isEmpty && trail.isEmpty {
                reset()                                   // isolated -> independent vowel
                return .commit(Engine.indep[unit] ?? unit)
            }
            vowel = combineVowel(vowel, unit)
            return .compose(render())
        }
        // Trailing sign (ং ঃ ঁ)
        if Engine.trailSigns.contains(unit) {
            trail += unit
            return .compose(render())
        }
        // Independent vowel / khanda-ta — a complete letter; commit the lot.
        if Engine.independents.contains(unit) || Engine.standalone.contains(unit) {
            let out = renderFinal() + unit; reset()
            return .commit(out)
        }
        // Consonant
        if isConsonant(unit) {
            if cons.isEmpty {
                // No base yet — this consonant is the base (and picks up any prebase
                // vowel already typed: ে then ক -> কে).
                cons = unit
                return .compose(render())
            }
            if endsWithHasanta(cons) {
                // Mid-conjunct (just typed ্): continue the cluster even if a prebase
                // vowel is pending — it rides through to the end (ি ক ্ ষ -> ক্ষি).
                cons += unit
                return .compose(render())
            }
            // Cluster already closed (no trailing ্): this consonant starts a NEW syllable.
            let out = renderFinal(); reset(); cons = unit
            return .commitThenCompose(out, render())
        }
        // Digits, punctuation, danda, currency, etc. — passthrough: flush + insert.
        let out = renderFinal() + unit; reset()
        return .commit(out)
    }

    private func popLast() {
        if !trail.isEmpty { trail.removeLast() }
        else if !vowel.isEmpty { vowel.removeLast() }
        else if !cons.isEmpty { cons.removeLast() }
    }

    // MARK: key map (keyCode + shift -> Bangla unit) — same key positions as the layout
    private struct Key: Hashable { let c: UInt16; let s: Bool; init(_ c: UInt16, _ s: Bool){self.c=c;self.s=s} }
    private static let map: [Key: String] = {
        var m: [Key: String] = [:]
        func n(_ c: UInt16, _ v: String){ m[Key(c,false)] = v }
        func sh(_ c: UInt16, _ v: String){ m[Key(c,true)]  = v }
        // normal layer
        n(0,"ৃ"); n(1,"ু"); n(2,"ি"); n(3,"া"); n(4,"ব"); n(5,"্"); n(6,"্র"); n(7,"ও")
        n(8,"ে"); n(9,"র"); n(11,"ন"); n(12,"ঙ"); n(13,"য"); n(14,"ড"); n(15,"প"); n(16,"চ")
        n(17,"ট"); n(18,"১"); n(19,"২"); n(20,"৩"); n(21,"৪"); n(22,"৬"); n(23,"৫"); n(25,"৯")
        n(26,"৭"); n(28,"৮"); n(29,"০"); n(31,"গ"); n(32,"জ"); n(34,"হ"); n(35,"ড়"); n(37,"দ")
        n(38,"ক"); n(40,"ত"); n(42,"ৎ"); n(45,"স"); n(46,"ম"); n(50,"—")
        // punctuation that maps to itself on the normal layer
        n(24,"="); n(27,"-"); n(30,"]"); n(33,"["); n(39,"'"); n(41,";"); n(43,","); n(44,"/"); n(47,".")
        // shift layer
        sh(0,"র্"); sh(1,"ূ"); sh(2,"ী"); sh(3,"অ"); sh(4,"ভ"); sh(5,"।"); sh(6,"্য"); sh(7,"ৗ")
        sh(8,"ৈ"); sh(9,"ল"); sh(11,"ণ"); sh(12,"ং"); sh(13,"য়"); sh(14,"ঢ"); sh(15,"ফ"); sh(16,"ছ")
        sh(17,"ঠ"); sh(19,"°"); sh(21,"৳"); sh(26,"ঁ"); sh(31,"ঘ"); sh(32,"ঝ"); sh(34,"ঞ")
        sh(35,"ঢ়"); sh(37,"ধ"); sh(38,"খ"); sh(40,"থ"); sh(42,"ঃ"); sh(45,"ষ"); sh(46,"শ")
        // shift punctuation -> itself
        sh(18,"!"); sh(20,"#"); sh(22,"^"); sh(23,"%"); sh(24,"+"); sh(25,"("); sh(27,"_")
        sh(28,"*"); sh(29,")"); sh(30,"}"); sh(33,"{"); sh(39,"\""); sh(41,":"); sh(43,"<")
        sh(44,"?"); sh(47,">"); sh(50,"‚")
        return m
    }()
}
