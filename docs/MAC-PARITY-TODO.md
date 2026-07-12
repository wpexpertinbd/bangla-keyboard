# Bangla Keyboard — macOS parity TODO

> For the **macOS Claude**. The shared engine/layouts are already at parity (both
> platforms are driven by the same macOS `.keylayout` — macOS is the source of truth).
> **Voice typing now exists on BOTH** (macOS `Bangla Voice.app`; Windows `win-v1.1.3`).
> The remaining gap is the **punctuation + fallback refinements Windows added in
> v1.1.2/v1.1.3** — see **§2** (the macOS app shipped the older auto-append behavior,
> which is now superseded). §1 documents the original feature for reference.
>
> Release channels stay separate: **macOS = `vX.Y.Z` (one is "Latest")**, **Windows =
> `win-vX.Y.Z`**. A Windows release must not steal "Latest" from the Mac `.pkg/.dmg`.
> Full Windows implementation notes live in the project memory + `SECURITY.md`.

---

## ✅ Already at parity
- **Engine + both layouts** — Bangla Unicode + Bangla Classic, byte-identical (the
  Windows `KLEngine` runs FSM tables generated from the Mac `.keylayout`).
- **De-branding** — no legacy-keyboard or legacy-ANSI-font brand/trademark names anywhere (legal). Say "fixed Windows-style layout" + "legacy ANSI font". Keep it that way.

---

## 1. Voice typing — speak Bangla + English (Windows `win-v1.1.1`)

> **✅ macOS port DONE — `macos/voice/` (`Bangla Voice.app`), pending real-device test.**
> Menu-bar agent: ⌃⌥S Bangla / ⌃⌥D English; `AVAudioEngine` capture + the same RMS/700 ms
> VAD as `voice.html`; 16 kHz L16 → Google `bn-BD` (fallback `bn-IN`) / `en-US`; `CGEvent`
> Unicode injection; blue/green menu-bar mic. Builds + launches + logic unit-tested on
> macOS 26. **v1 uses the uniform HTTP path for BOTH languages** (only Microphone permission
> needed); the `SFSpeechRecognizer` live-English upgrade below is left as a future option.
> Deviation: the menu-bar item is **persistent** (dim when off) rather than removed-when-idle,
> because on macOS it's the app's only control surface (start/quit). Still TODO: user grants
> Mic + Accessibility and verifies live recognition/injection; then fold into the DMG + cut a
> macOS release.

The headline feature to port. **Everything is free — no paid API, nothing stored.**

### Behavior to match
- **Two language voice modes**, each its own hotkey + menu item (Windows:
  `Ctrl+Alt+S` Bangla, `Ctrl+Alt+D` English → on macOS use `⌃⌥S` / `⌃⌥D` or
  menu-bar items). NOT auto-detect — see the gotcha below.
- Speak → recognized text is **inserted at the cursor in any app**. (⚠️ The original
  build auto-appended a `।`/`.` per utterance — that's **superseded by §2**; do the
  spoken-punctuation rule instead.)
- A **menu-bar (status-item) microphone indicator shown ONLY while listening**, color
  by state — **blue = listening, green = hearing speech** — and removed when you stop.
  No on-screen window.
- **Opt-in** at install / first run, labelled "requires internet".

### The engine design (copy this — it's what makes correct bn-BD free)
1. **English** — prefer the OS's free recognizer: **`SFSpeechRecognizer` (Speech.framework)**
   with `en-US` if available (live, on-device on recent macOS). Otherwise use the HTTP
   path in (3).
2. **Bangla — try `SFSpeechRecognizer` first, but VERIFY THE SCRIPT.** Check
   `SFSpeechRecognizer.supportedLocales()` for `bn-BD` / `bn-IN`. ⚠️ The hard-won
   Windows lesson: a Bengali recognizer that isn't truly **bn-BD** gives *Indian*
   Bangla vocabulary, which the user rejects. If Apple's recognizer doesn't give
   correct **Bangladeshi** Bangla, do NOT use it for Bangla — use (3).
3. **Bangla fallback = the same free Google endpoint Windows uses** (correct bn-BD):
   record **16 kHz mono PCM**, POST each utterance to
   `https://www.google.com/speech-api/v2/recognize?output=json&lang=bn-BD&key=<key>`
   with `Content-Type: audio/l16; rate=16000` (**raw L16 PCM — no FLAC encoder
   needed**), parse the `"transcript"` from the line-delimited JSON, insert it. The
   endpoint + key + parser are in `windows/voice/stt_http.h` — port verbatim to
   `URLSession` (native code has no CORS problem). It is **unofficial + rate-limited**,
   so on an empty/throttled result **fall back** (to `bn-IN` or `SFSpeechRecognizer`)
   so typing never breaks — exactly as Windows does.

### Windows source to diff against
- **`windows/voice/stt_http.h`** — `stt::googleSTT(pcm, len, rate, lang)`: the WinHTTP
  POST + UTF-8 `"transcript"` extraction + the public key. → macOS `URLSession` dataTask.
- **`windows/voice/voice.html`** — the capture + VAD logic to mirror natively:
  `ScriptProcessorNode` per-buffer **RMS energy gate** (`> 0.012` = speech), collect
  while speaking, **`> 700 ms` trailing silence = utterance end**, concat →
  **linear-downsample to 16 kHz Int16** → send. Also the punctuation/capitalize rules.
- **`windows/voice/voicehost.cpp`** — orchestration to mirror: per-utterance STT on a
  background thread, `s:speak`/`s:idle` → tray icon color, fallback wiring, inject.
- **`windows/tray/tray.cpp`** — the menu items (`Bangla Voice` / `English Voice` with
  mic icons) + how the state icon is shown only while listening.

### macOS implementation guidance
- **Capture:** `AVAudioEngine` input tap → `AVAudioConverter` to 16 kHz mono Int16.
  Mirror `voice.html`'s RMS gate + 700 ms-silence VAD for utterance boundaries.
- **Insert text:** use the same path the keyboard app already uses to emit characters
  (e.g. `CGEventPost` Unicode keystrokes), so it works in any app. Strip control chars
  from STT text before inserting (Windows does — a bad response shouldn't inject Return/Tab).
- **Indicator:** `NSStatusItem` with a colored mic image, added on start / removed on stop.
- **Permissions:** `NSMicrophoneUsageDescription` (+ `NSSpeechRecognitionUsageDescription`
  if using `SFSpeechRecognizer`) in Info.plist; request authorization before first use.
- **Privacy/security (match `SECURITY.md`):** mic only while listening, **nothing stored**
  (no audio/transcript to disk, no logging), network scope = the speech service only,
  opt-in so users who don't want cloud STT can omit it entirely.

### Gotchas carried over from the Windows build
- **`bn-IN` ≠ `bn-BD`.** bn-IN returns Indian vocabulary; the Google L16 endpoint gives
  correct Bangladeshi bn-BD (verified). This is the whole reason for the HTTP path.
- **Raw L16 works** on that endpoint — no FLAC. (The Python `recognize_google` lib uses
  FLAC, but L16 is accepted and far simpler.)
- **The free endpoint is unofficial + throttles** under load → always keep a fallback.
- **No auto-detect.** Two simultaneous recognizers on one mic conflict, and an English
  recognizer confidently *romanizes* Bangla (so confidence can't pick the language).
  Two explicit language modes is the reliable, user-approved design.
- **HTTP path has ~1 s latency, no live preview** (record → POST → insert). A native
  `SFSpeechRecognizer` path (if it gives correct script) *is* live — prefer it where it
  qualifies, HTTP where it doesn't.

---

## 2. ⭐ Punctuation + reliable fallback (Windows `win-v1.1.2` / `v1.1.3`) — DO THIS

The free online STT returns **no punctuation**, and free *auto*-punctuation doesn't exist
(Google Cloud `enableAutomaticPunctuation` is paid; offline XLM-RoBERTa Bangla restorer is
~2 GB). So Windows does what Google Docs/Gboard English voice typing does — **the user
SPEAKS the mark** — and learned three things the macOS app should copy:

### 2a. Spoken punctuation (replace the auto-append `।`/`.`)
Map the spoken word → mark, **don't** auto-append anything on a pause:
- Bangla : `দাঁড়ি`→`।`, `কমা`→`,`, `প্রশ্ন`→`?`, `বিস্ময়`→`!`, `সেমিকোলন`→`;`, `কোলন`→`:`
- English: `comma`→`,`, `period`/`full stop`→`.`, `question mark`→`?`, `exclamation mark`→`!`
- **Include misheard variants** — Google often hears `কমা` as **`কহনা`** (also `কহানা`,
  `কম্মা`); `দাঁড়ি` as `দাড়ি`/`দাড়ী`; `প্রশ্ন` as `প্রসন`. Map them all.

### 2b. Convert ONLY when the mark word is said ALONE (the key fix — `v1.1.3`)
A mark is produced **only when the WHOLE (trimmed) utterance is exactly the command
word** — said alone, after a pause. Anything with other words passes through verbatim, so
a real word inside a sentence is never clobbered:
- "আমার একটা **প্রশ্ন** আছে" (one breath) → stays text (no `?`). `দাড়ি` = *beard* stays too.
- "তুমি কেমন আছো" → *pause* → "**প্রশ্ন**" → "তুমি কেমন আছো?"
So **exact-match the trimmed utterance, NOT a per-token replace.** And when a standalone
mark is injected, **backspace the trailing space** from the previous utterance so it
attaches ("…আছি" + "।" → "…আছি।"). Trade-off (acceptable): a comma list needs a pause
before each "কমা".

### 2c. Fall back to bn-IN only on a REAL HTTP error, not on silence (`v1.1.2`)
An empty STT result means *silence* (HTTP 200, no result) far more often than a throttle.
Counting empties as failures made a brief pause wrongly flip Bangla to the weaker bn-IN
recognizer. Have the STT call report a real error separately (read the **HTTP status code**;
true only on network / 4xx / 5xx) and fall back only after N (≈3) real errors. On macOS:
check `(response as? HTTPURLResponse)?.statusCode` in the `URLSession` completion.

### Windows source to diff against
`windows/voice/voicehost.cpp` → `applyBanglaPunct` (2a/2b) + `injectSmart` (leading-mark
backspace); `windows/voice/voice.html` → `applyPunct` (en/bn-IN, same rule);
`windows/voice/stt_http.h` → `googleSTT(..., bool* httpErr)` (2c, status-code check).

---

## 3. Keep the release channels separate
When cutting a macOS release, keep the macOS `vX.Y.Z` tag as **Latest**. Windows ships
under `win-vX.Y.Z` with `--latest=false`. (See the BHServe repo's `docs/WINDOWS-RELEASE.md`
for the canonical multi-platform release flow this mirrors.) ⚠️ The platforms' Claudes both
push `main` — expect non-fast-forward pushes; `git fetch` + `rebase origin/main` (the README
platforms-table rows conflict — keep BOTH), then move the tag to the rebased commit.
