# Bangla Keyboard — macOS parity TODO

> For the **macOS Claude**. The shared engine/layouts are already at parity (both
> platforms are driven by the same macOS `.keylayout` — macOS is the source of truth).
> What the **Windows** build now has that macOS doesn't: **free voice typing**
> (shipped `win-v1.1.1`). This doc is how to add the *same* feature to the macOS app.
>
> Release channels stay separate: **macOS = `vX.Y.Z` (one is "Latest")**, **Windows =
> `win-vX.Y.Z`**. A Windows release must not steal "Latest" from the Mac `.pkg/.dmg`.
> Full Windows implementation notes live in the project memory + `SECURITY.md`.

---

## ✅ Already at parity
- **Engine + both layouts** — Bangla Unicode + Bangla Classic, byte-identical (the
  Windows `KLEngine` runs FSM tables generated from the Mac `.keylayout`).
- **De-branding** — no "Bijoy"/"SutonnyMJ" text anywhere (legal). Keep it that way.

---

## 1. Voice typing — speak Bangla + English (Windows `win-v1.1.1`)

The headline feature to port. **Everything is free — no paid API, nothing stored.**

### Behavior to match
- **Two language voice modes**, each its own hotkey + menu item (Windows:
  `Ctrl+Alt+S` Bangla, `Ctrl+Alt+D` English → on macOS use `⌃⌥S` / `⌃⌥D` or
  menu-bar items). NOT auto-detect — see the gotcha below.
- Speak → recognized text is **inserted at the cursor in any app**. A **`।`** (Bangla)
  or **`.`** (English) is auto-appended at the end of each utterance; English first
  letter is capitalized.
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

## 2. Keep the release channels separate
When cutting a macOS release, keep the macOS `vX.Y.Z` tag as **Latest**. Windows ships
under `win-vX.Y.Z` with `--latest=false`. (See the BHServe repo's `docs/WINDOWS-RELEASE.md`
for the canonical multi-platform release flow this mirrors.)
