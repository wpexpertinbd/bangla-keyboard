#!/usr/bin/env python3
"""Generate `Bangla Phonetic.keylayout` — a phonetic (roman -> Bangla) macOS keyboard layout.

Type Bangla the way it sounds: `ami` -> আমি, `bhalO` -> ভালো, `korrmo` -> কর্ম.
The conventions follow the long-established phonetic scheme Bangladeshi typists already
know; the rules and this code are our own (MIT) — no third-party rule file or code is used.

How it works: a keylayout is a dead-key state machine. Each state = (left context, pending
roman, word-start tracker). Roman that may still change (`k` may become `kh`/`kk`/`kkh`) is
held as pending and shown live as marked text (the terminator), so `ami` shows আমি while typing.
Run:  python3 gen_phonetic.py [out.keylayout]
"""
import re, sys, os
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
UNICODE_SRC = os.path.join(HERE, "..", "keylayouts", "Bangla Unicode.keylayout")
DEFAULT_OUT = os.path.join(HERE, "..", "keylayouts", "Bangla Phonetic.keylayout")
KEYBOARD_ID = "-19020"
HAS = "\u09CD"          # hasanta ্
NUKTA = "\u09BC"        # nukta ়

# ---------------------------------------------------------------- rules (ours, MIT) ----
CONS = {  # roman -> one consonant
    "k": "ক", "kh": "খ", "g": "গ", "G": "গ", "gh": "ঘ", "Gh": "ঘ", "Ng": "ঙ",
    "c": "চ", "ch": "ছ", "j": "জ", "jh": "ঝ", "NG": "ঞ",
    "T": "ট", "Th": "ঠ", "D": "ড", "Dh": "ঢ", "N": "ণ",
    "t": "ত", "th": "থ", "d": "দ", "dh": "ধ", "n": "ন",
    "p": "প", "ph": "ফ", "f": "ফ", "b": "ব", "bh": "ভ", "v": "ভ", "m": "ম",
    "z": "য", "r": "র", "l": "ল", "sh": "শ", "S": "শ", "Sh": "ষ", "s": "স", "h": "হ",
    "R": "\u09DC", "Rh": "\u09DD", "Y": "\u09DF", "J": "জ" + NUKTA, "q": "ক",
}
CLUSTERS = {  # roman -> conjunct whose FIRST letter differs from the plain reading
    "kkh": "ক্ষ", "kx": "ক্ষ", "x": "ক্স", "gg": "জ্ঞ",
    "nk": "ঙ্ক", "nkh": "ঙ্খ", "ngg": "ঙ্গ",
    "nc": "ঞ্চ", "nch": "ঞ্ছ", "nj": "ঞ্জ", "njh": "ঞ্ঝ",
}
VOWELS = {  # roman -> (independent, vowel sign)
    "o": ("অ", ""), "a": ("আ", "া"), "i": ("ই", "ি"), "I": ("ঈ", "ী"), "ee": ("ঈ", "ী"),
    "u": ("উ", "ু"), "oo": ("উ", "ু"), "U": ("ঊ", "ূ"), "rri": ("ঋ", "ৃ"), "e": ("এ", "ে"),
    "OI": ("ঐ", "ৈ"), "O": ("ও", "ো"), "OU": ("ঔ", "ৌ"),
}
VOWEL_START = set("oaiIeuUO")
# Consonant pairs that join automatically (attested Bangla conjuncts). Ya/ba-fola come
# from `y`/`w`/`Z`; reph from `rr`; anything else can be forced with `,,` (hasanta).
PAIRS_STR = """
ক্ক ক্ট ক্ত ক্ম ক্র ক্ল ক্স খ্র গ্ণ গ্ধ গ্ন গ্ম গ্র গ্ল ঘ্ন ঘ্র ঙ্ক ঙ্খ ঙ্গ ঙ্ঘ ঙ্ম
চ্চ চ্ছ চ্ঞ জ্জ জ্ঝ জ্ঞ জ্র ঞ্চ ঞ্ছ ঞ্জ ঞ্ঝ ট্ট ট্ম ট্র ড্ড ড্র ঢ্র
ণ্ট ণ্ঠ ণ্ড ণ্ঢ ণ্ণ ণ্ন ণ্ম ত্ত ত্থ ত্ন ত্ম ত্র থ্র দ্গ দ্ঘ দ্দ দ্ধ দ্ভ দ্ম দ্র ধ্ন ধ্ম ধ্র
ন্ট ন্ঠ ন্ড ন্ত ন্থ ন্দ ন্ধ ন্ন ন্ম ন্স প্ট প্ত প্ন প্প প্র প্ল প্স ফ্র ফ্ল
ব্জ ব্দ ব্ধ ব্ব ব্র ব্ল ভ্র ভ্ল ম্ন ম্প ম্ফ ম্ব ম্ভ ম্ম ম্র ম্ল ম্থ য্য
ল্ক ল্গ ল্ট ল্ড ল্ধ ল্প ল্ব ল্ভ ল্ম ল্ল শ্চ শ্ছ শ্ত শ্ন শ্ম শ্র শ্ল
ষ্ক ষ্ট ষ্ঠ ষ্ণ ষ্প ষ্ফ ষ্ম স্ক স্খ স্ট স্ত স্থ স্ন স্প স্ফ স্ম স্র স্ল হ্ণ হ্ন হ্ম হ্র হ্ল
"""
# Word-start exceptions: the consonant that ENDS one of these prefixes does NOT join the
# one before it. Covers the very common words where the join rule gives the wrong spelling
# (আমরা not আম্রা, একটা not এক্টা, আপনি not আপ্নি). Anywhere else, type ` between the
# consonants to keep them apart (am`ra).
EXCEPTIONS = ["amr", "tOmr", "kamr", "jamr", "ekT", "apn"]

DIGITS = dict(zip("0123456789", "০১২৩৪৫৬৭৮৯"))
DIRECT_SPECIAL = {"$": "৳", "^": "ঁ"}

def units(s):
    """split a consonant string into consonant units (consonant + optional nukta)"""
    out = []
    for ch in s:
        if ch == HAS: continue
        if ch == NUKTA and out: out[-1] += ch
        else: out.append(ch)
    return out

PAIRS = set()
for w in PAIRS_STR.split():
    u = units(w)
    assert len(u) == 2, w
    PAIRS.add((u[0], u[1]))

TOK = {}
for r, b in CONS.items(): TOK[r] = ("C", b)
for r, b in CLUSTERS.items(): TOK[r] = ("C", b)
for r, (i, m) in VOWELS.items():
    TOK[r] = ("V", i, m)
    TOK[r + "`"] = ("VF", m)                 # vowel + ` = force the vowel sign
TOK["aZ"] = ("V", "অ্যা", "্যা")
TOK.update({"y": ("Y",), "w": ("W",), "Z": ("Z",), "ng": ("NG",), "rr": ("REPH",),
            ",,": ("HAS",), ",": ("P", ","), ".": ("P", "।"), ".`": ("P", "."),
            ":": ("SIGN", "ঃ"), ":`": ("P", ":"), "`": ("BRK",), "t``": ("SIGN", "ৎ")})

PREFIXES = set()
for t in TOK:
    for k in range(1, len(t)):
        PREFIXES.add(t[:k])
ACTION_CHARS = sorted({ch for t in TOK for ch in t})
for ch in ACTION_CHARS:
    assert ch in TOK, "every action char must itself be a token: %r" % ch
UPPER_SENSITIVE = {ch for ch in ACTION_CHARS if ch.isupper()}

def longest_token_prefix(q):
    for k in range(len(q), 0, -1):
        if q[:k] in TOK: return q[:k]
    return None

def emit(m, ctx, nxt, pre):
    """output + new context for token m. ctx: ('S',) start, ('B',) after `, ('V',) after a
    vowel/sign, ('H',) after hasanta/reph, ('C', unit) after a consonant."""
    sp = TOK[m]; kind = sp[0]; c0 = ctx[0]
    if kind == "C":
        s = sp[1]; u = units(s)
        if c0 == "C" and (ctx[1], u[0]) in PAIRS and pre not in EXCEPTIONS:
            return HAS + s, ("C", u[-1])
        return s, ("C", u[-1])
    if kind == "V":
        indep, sign = sp[1], sp[2]
        if m == "o":
            if c0 == "C": return "", ("V",)          # inherent vowel: nothing written
            if c0 == "V": return "ও", ("V",)
            return "অ", ("V",)
        if c0 == "C": return sign, ("V",)
        if c0 == "V" and m == "a": return "য়া", ("V",)  # সামিয়া, খাওয়া
        return indep, ("V",)
    if kind == "VF": return sp[1], ("V",)
    if kind == "Y":
        if c0 == "C": return HAS + "য", ("C", "য")       # ya-fola
        if c0 in ("S", "B"): return "ইয়", ("C", "\u09DF")  # word start: ইয়াহু
        return "\u09DF", ("C", "\u09DF")
    if kind == "W":
        if c0 == "C": return HAS + "ব", ("C", "ব")       # ba-fola
        return "ও", ("V",)
    if kind == "Z": return HAS + "য", ("C", "য")
    if kind == "NG":
        if nxt is not None and nxt in VOWEL_START: return "ঙ", ("C", "ঙ")   # রঙিন
        return "ং", ("V",)                                                  # বাংলা
    if kind == "REPH": return "র" + HAS, ("H",)
    if kind == "HAS": return HAS, ("H",)
    if kind == "BRK": return "", ("B",)
    if kind == "P": return sp[1], ("S",)
    if kind == "SIGN": return sp[1], ("V",)
    raise ValueError(m)

def _next_word(word, c):
    if not c.isalpha(): return ""
    if word is None: return None
    nw = word + c
    return nw if any(e.startswith(nw) for e in EXCEPTIONS) else None

def process(state, c):
    """one action keystroke -> (output, new state)"""
    ctx, pend, word = state
    q = pend + c
    run = None if word is None else (word + c if c.isalpha() else word)
    out = ""
    while q:
        if q in PREFIXES: break                       # may still grow: keep pending
        m = longest_token_prefix(q)
        if m is None:
            out += q[0]; ctx = ("S",); q = q[1:]; continue
        rest = q[len(m):]
        nxt = rest[0] if rest else None
        pre = None
        if run is not None:
            k = len(rest) if c.isalpha() else len(rest) - 1
            if 0 <= k <= len(run) and all(ch.isalpha() for ch in rest[:k]):
                pre = run[:len(run) - k]
        o, ctx = emit(m, ctx, nxt, pre)
        out += o; q = rest
    return out, (ctx, q, _next_word(word, c))

def flush(state):
    """what the pending roman becomes when the word ends (= the dead state's terminator)"""
    ctx, q, run = state
    out = ""
    while q:
        m = longest_token_prefix(q)
        if m is None:
            out += q[0]; ctx = ("S",); q = q[1:]; continue
        rest = q[len(m):]
        pre = run[:len(run) - len(rest)] if run is not None and len(rest) <= len(run) else None
        o, ctx = emit(m, ctx, rest[0] if rest else None, pre)
        out += o; q = rest
    return out

NONE = (("S",), "", "")

def keychar(ch):
    """case-insensitive letters: Shift+k types the same as k"""
    if ch.isupper() and ch not in UPPER_SENSITIVE: return ch.lower()
    return ch

def direct(ch):
    return DIGITS.get(ch, DIRECT_SPECIAL.get(ch, ch))

def type_ref(text):
    """reference: what typing `text` produces (same semantics as the macOS engine)"""
    st = NONE; out = ""
    for ch in text:
        ch = keychar(ch)
        if ch in ACTION_CHARS:
            o, st = process(st, ch); out += o
        else:
            out += flush(st) + direct(ch); st = NONE
    return out + flush(st)

# ---------------------------------------------------------------- state machine ----
def build_fsm():
    trans, order, seen = {}, [NONE], {NONE}
    dq = deque([NONE])
    while dq:
        s = dq.popleft()
        for c in ACTION_CHARS:
            o, ns = process(s, c)
            trans[(s, c)] = (o, ns)
            if ns not in seen:
                seen.add(ns); order.append(ns); dq.append(ns)
    return minimize(order, trans)

def minimize(order, trans):
    """Merge equivalent states (Moore partition refinement): same terminator, and for every
    key the same output + equivalent next state. Exact — typed output cannot change."""
    cls = {s: flush(s) for s in order}
    n = len(set(cls.values()))
    while True:                       # each round refines the previous partition
        sig = {s: (cls[s],) + tuple((trans[(s, c)][0], cls[trans[(s, c)][1]]) for c in ACTION_CHARS)
               for s in order}
        ids = {}
        new = {s: ids.setdefault(sig[s], len(ids)) for s in order}
        if len(ids) == n: break       # no class split -> stable
        n, cls = len(ids), new
    rep = {}
    for s in order:                                   # NONE comes first, so it represents its class
        rep.setdefault(cls[s], s)
    reps = [s for s in order if rep[cls[s]] == s]
    t2 = {(s, c): (trans[(s, c)][0], rep[cls[trans[(s, c)][1]]]) for s in reps for c in ACTION_CHARS}
    return reps, t2

# ---------------------------------------------------------------- keylayout xml ----
BASE = {'a':0,'s':1,'d':2,'f':3,'h':4,'g':5,'z':6,'x':7,'c':8,'v':9,'b':11,'q':12,'w':13,'e':14,
        'r':15,'y':16,'t':17,'1':18,'2':19,'3':20,'4':21,'6':22,'5':23,'=':24,'9':25,'7':26,'-':27,
        '8':28,'0':29,']':30,'o':31,'u':32,'[':33,'i':34,'p':35,'l':37,'j':38,"'":39,'k':40,';':41,
        '\\':42,',':43,'/':44,'n':45,'m':46,'.':47,'`':50,' ':49}
SHIFT = {'1':'!','2':'@','3':'#','4':'$','5':'%','6':'^','7':'&','8':'*','9':'(','0':')','-':'_',
         '=':'+','[':'{',']':'}','\\':'|',';':':',"'":'"',',':'<','.':'>','/':'?','`':'~',' ':' '}
PRINTABLE = set(BASE.values()) | {10, 93, 94, 95}

def xesc(s):
    out = []
    for ch in s:
        o = ord(ch)
        if ch == "&": out.append("&amp;")
        elif ch == "<": out.append("&lt;")
        elif ch == ">": out.append("&gt;")
        elif ch == '"': out.append("&quot;")
        elif o < 0x20 or o == 0x7F: out.append("&#x%04X;" % o)
        else: out.append(ch)
    return "".join(out)

ACT_ID = {c: c for c in ACTION_CHARS}
ACT_ID.update({"`": "bt", ",": "com", ".": "dot", ":": "col"})
for c in ACTION_CHARS:
    if c.isupper(): ACT_ID[c] = "U" + c                 # action ids: keep case unambiguous

def key_entry(code, ch, phonetic=True):
    """keyMap line for a typed character"""
    if phonetic:
        k = keychar(ch)
        if k in ACTION_CHARS:
            return '            <key code="%d" action="%s"/>' % (code, ACT_ID[k])
        return '            <key code="%d" output="%s"/>' % (code, xesc(direct(k)))
    return '            <key code="%d" output="%s"/>' % (code, xesc(ch))

def generate(out_path):
    src = open(UNICODE_SRC, encoding="utf-8").read()
    header = src.split('<keyMapSet id="ANSI">')[0]
    header = re.sub(r"<!--Last edited.*?-->\n", "", header)
    ansi_src = src.split('<keyMapSet id="ANSI">')[1].split("</keyMapSet>")[0]
    jis_src = src.split('<keyMapSet id="JIS">')[1].split("</keyMapSet>")[0]
    src_maps = {int(i): b for i, b in re.findall(r'<keyMap index="(\d+)">(.*?)</keyMap>', ansi_src, re.S)}
    jis_full = {int(i): b for i, b in re.findall(r'<keyMap index="(\d+)">(.*?)</keyMap>', jis_src, re.S)}

    def special_keys(idx):          # Return/Tab/Delete/arrows/F-keys/numpad from the Unicode layout
        lines = []
        for m in re.finditer(r'<key code="(\d+)" (output|action)="([^"]*)"\s*/>', src_maps[idx]):
            if int(m.group(1)) not in PRINTABLE:
                assert m.group(2) == "output"
                lines.append('            <key code="%s" output="%s"/>' % (m.group(1), m.group(3)))
        return lines

    order, trans = build_fsm()
    name = {s: ("none" if s == NONE else "s%d" % i) for i, s in enumerate(order)}

    keymaps = []
    for idx in range(5):
        lines = []
        for ch, code in sorted(BASE.items(), key=lambda kv: kv[1]):
            # Caps Lock alone (3) = plain: case changes meaning here (t/T, o/O, r/R), so an
            # accidental Caps Lock would garble every word. Shift still gives the capitals.
            if idx in (0, 3): lines.append(key_entry(code, ch))                               # plain
            elif idx == 1: lines.append(key_entry(code, ch.upper() if ch.isalpha() else SHIFT[ch]))  # shift
            elif idx == 2: lines.append(key_entry(code, ch, phonetic=False))                  # option: ASCII
            else:          lines.append(key_entry(code, ch.upper() if ch.isalpha() else SHIFT[ch], phonetic=False))
        lines += special_keys(idx)
        keymaps.append('        <keyMap index="%d">\n%s\n        </keyMap>\n' % (idx, "\n".join(lines)))
    for idx in (5, 6):   # Cmd / Cmd+Shift = plain ASCII so shortcuts work (copied verbatim)
        keymaps.append('        <keyMap index="%d">%s</keyMap>\n' % (idx, src_maps[idx]))

    jis = "".join('        <keyMap index="%d" baseMapSet="ANSI" baseIndex="%d">\n'
                  '            <key code="512" output=""/>\n        </keyMap>\n' % (i, i) for i in range(5))
    jis += "".join('        <keyMap index="%d">%s</keyMap>\n' % (i, jis_full[i]) for i in (5, 6))

    # actions: only entries that differ from the engine's fallback (terminator + none-path)
    maxout = 1
    act_lines = []
    for c in ACTION_CHARS:
        o0, ns0 = trans[(NONE, c)]
        body = ['            <when state="none"%s%s/>' % (
            ' output="%s"' % xesc(o0) if o0 else "", ' next="%s"' % name[ns0] if ns0 != NONE else "")]
        maxout = max(maxout, len(o0.encode("utf-16-le")) // 2)
        for s in order:
            if s == NONE: continue
            o, ns = trans[(s, c)]
            if (o, ns) == (flush(s) + o0, ns0): continue
            maxout = max(maxout, len(o.encode("utf-16-le")) // 2)
            body.append('            <when state="%s"%s%s/>' % (
                name[s], ' output="%s"' % xesc(o) if o else "", ' next="%s"' % name[ns] if ns != NONE else ""))
        act_lines.append('        <action id="%s">\n%s\n        </action>' % (ACT_ID[c], "\n".join(body)))
    term_lines = []
    for s in order:
        if s == NONE: continue
        f = flush(s)
        if f:
            maxout = max(maxout, len(f.encode("utf-16-le")) // 2)
            term_lines.append('        <when state="%s" output="%s"/>' % (name[s], xesc(f)))

    header = re.sub(r'<keyboard group="(\d+)" id="-?\d+" name="[^"]*" maxout="\d+">',
                    '<keyboard group="\\1" id="%s" name="Bangla Phonetic" maxout="%d">' % (KEYBOARD_ID, maxout),
                    header)
    header = re.sub(r'(<!DOCTYPE[^>]*>\n)',
                    '\\1<!--Generated by macos/src/phonetic/gen_phonetic.py (MIT). Do not edit by hand.-->\n',
                    header, count=1)
    xml = (header + '<keyMapSet id="ANSI">\n' + "".join(keymaps) + '    </keyMapSet>\n'
           '    <keyMapSet id="JIS">\n' + jis + '    </keyMapSet>\n'
           '    <actions>\n' + "\n".join(act_lines) + '\n    </actions>\n'
           '    <terminators>\n' + "\n".join(term_lines) + '\n    </terminators>\n</keyboard>\n')
    open(out_path, "w", encoding="utf-8").write(xml)
    return len(order), maxout, sum(len(a.splitlines()) - 2 for a in act_lines)

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT
    n, mo, nw = generate(out)
    print("wrote %s: %d states, %d action entries, maxout=%d" % (out, n, nw, mo))
