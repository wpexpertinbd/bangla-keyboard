# Bangla Phonetic — cheat sheet

Type Bangla the way it **sounds**. Works the same on **macOS, Windows and Linux** — it's
the same layout on all three.

> Every example on this page was run through the real engine, so what you see is exactly
> what you get. The full regression corpus lives in
> [`macos/src/phonetic/tests.tsv`](../macos/src/phonetic/tests.tsv).

**Switch to it:** Windows **Ctrl+Alt+P** · Linux **Super+Space** (after adding *Bangla
(Phonetic)*) · macOS **⌃Space**.

## The 5 rules that matter

| # | Rule | Example |
|---|------|---------|
| 1 | **`o` is the built-in vowel, `O` is ো** | `bhalo` → ভাল · `bhalO` → **ভালো** |
| 2 | **Capitals are different letters** for `t/T d/D n/N r/R s/S i/I u/U o/O j/J y/Y z/Z G` | `Taka` → টাকা · `baRi` → বাড়ি · `bhaSha` → ভাষা |
| 3 | **Two consonants join automatically.** Don't want them joined? Put `` ` `` between them | `kolkata` → কল্কাতা ✗ · `` kol`kata `` → **কলকাতা** ✓ |
| 4 | **`rr` = র্ (reph)**, `y` after a consonant = ্য | `korrmo` → কর্ম · `bybohar` → ব্যবহার |
| 5 | **Long vowels are capitals**: `I` ী, `U` ূ, or double them | `jIbon` → জীবন · `dUr` → দূর · `kee` → কী |

Caps Lock on by accident? **Nothing breaks** — Caps Lock alone is ignored, so only the
Shift key changes a letter.

## Everyday words

| Type | You get | Type | You get |
|------|---------|------|---------|
| `ami` | আমি | `tumi` | তুমি |
| `hyalO` | হ্যালো | `TesT` | টেস্ট |
| `dhonyobad` | ধন্যবাদ | `swagotom` | স্বাগতম |
| `kemon achen` | কেমন আছেন | `abar dekha hobe` | আবার দেখা হবে |
| `assalamu alaikum` | আসসালামু আলাইকুম | `shubheccha` | শুভেচ্ছা |
| `bangladesh` | বাংলাদেশ | `Dhaka` | ঢাকা |
| `bhalObasi` | ভালোবাসি | `bondhu` | বন্ধু |
| `nam` | নাম | `somoy` | সময় |
| `pani` | পানি | `bhat` | ভাত |
| `Taka` | টাকা | `baRi` | বাড়ি |
| `sokal` | সকাল | `rat` | রাত |
| `ajke` | আজকে | `` kal`ke `` | কালকে |
| `boi` | বই | `poRalekha` | পড়ালেখা |
| `skul` | স্কুল | `` cak`ri `` | চাকরি |
| `ofis` | অফিস | `` dor`kar `` | দরকার |
| `mObail` | মোবাইল | `kompiuTar` | কম্পিউটার |
| `inTarneT` | ইন্টারনেট | `fesbuk` | ফেসবুক |
| `pasOyarrD` | পাসোয়ার্ড | `ingreji` | ইংরেজি |
| `sorkar` | সরকার | `khoborer kagoj` | খবরের কাগজ |
| `prrithibI` | পৃথিবী | `onuShThan` | অনুষ্ঠান |
| `sundorI` | সুন্দরী | `bishwo` | বিশ্ব |
| `bhaiyer` | ভাইয়ের | `jonyo` | জন্য |
| `rong` | রং | `bangali` | বাঙালি |
| `okkhor` | অক্ষর | `shikkhok` | শিক্ষক |
| `amra` | আমরা | `apni` | আপনি |

## Punctuation, digits, symbols

| Type | You get | | Type | You get |
|------|---------|---|------|---------|
| `.` | ।  (dari) | | `1234` | ১২৩৪ |
| `` .` `` | . (a real full stop) | | `100$` | ১০০৳ |
| `?` | ? | | `^` | ঁ  (`ca^d` → চাঁদ) |
| `:` | ঃ  (`du:kh` → দুঃখ) | | `,,` | ্  (hasanta: `` Dok,,Tor `` → ডক্টর) |

## If you get the wrong word

| You typed | You got | Type this instead | |
|-----------|---------|-------------------|---|
| `bhalo` | ভাল | `bhalO` | capital **O** makes ো |
| `hyalo` | হ্যাল | `hyalO` | same — it's the last letter |
| `kolkata` | কল্কাতা | `` kol`kata `` | `` ` `` keeps ল and ক apart |
| `bolben` | বল্বেন | `` bol`ben `` | same |
| `cakri` | চাক্রি | `` cak`ri `` | same |
| `dorrkar` | দর্কার | `` dor`kar `` | `rr` is **reph**, not "r then r" |
| `onushThan` | অনুশঠান | `onuShThan` | `Sh` is ষ, `sh` is শ |
| `school` | সছুল | `skul` | spell the **sound**, not the English |
| `internet` | ইন্তেরনেত | `inTarneT` | `T` is ট, `t` is ত |

## Can it autocorrect / suggest words?

No — and that's deliberate. This is a **keyboard layout**, not an input method with a
floating candidate bar. That means it never swallows your **arrow keys**, never breaks
**Shift+arrow** text selection, and nothing pops up over your screen while you type.
A layout can't carry a dictionary, so the most common words are built in as exceptions
(`amra` আমরা, `tOmra` তোমরা, `ekTa` একটা, `ekTu` একটু, `apni` আপনি); anywhere else, use
`` ` `` to keep two consonants apart.

Need English in the middle of Bangla? Switch back with the same shortcut — it's one
keypress (**Ctrl+Alt+P** / **Super+Space** / **⌃Space**).
