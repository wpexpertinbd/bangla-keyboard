# Bangla Voice (macOS) — free voice typing

The macOS port of the Windows voice-typing feature (`win-v1.1.1`). A **menu-bar agent app**
that types what you say, in any app — Bangla or English, free, nothing stored.

> The macOS keyboard itself is a passive `.keylayout` (no running process), so voice typing
> ships as this small companion app rather than being built into the layout.

## Use
- **⌃⌥S** — Bangla voice · **⌃⌥D** — English voice · press again (or the menu) to stop.
- Speak → the recognized text is inserted at the cursor.
- **Punctuation is spoken** (free STT returns none), like Google Docs / Gboard voice typing:
  say the mark **alone, after a short pause**. Bangla: `দাঁড়ি`→`।`, `কমা`→`,`, `প্রশ্ন`→`?`,
  `বিস্ময়`→`!`, `সেমিকোলন`→`;`, `কোলন`→`:`. English: “full stop”→`.`, “comma”→`,`,
  “question mark”→`?`. A mark is only inserted when it's the **whole** utterance, so the same
  word inside a sentence (“আমার একটা প্রশ্ন আছে”) stays as text.
- The menu-bar mic shows state: **dim = off, blue = listening, green = hearing speech**.

## How it works (parity with Windows — see [`../../SPEC.md`](../../SPEC.md) and [`../../docs/MAC-PARITY-TODO.md`](../../docs/MAC-PARITY-TODO.md))
- **Capture + VAD** ([`Sources/Audio.swift`](Sources/Audio.swift)) — `AVAudioEngine` tap, the
  same RMS energy gate (`> 0.012`) + `700 ms` trailing-silence utterance boundary as the
  Windows `voice.html`, downsampled to 16 kHz mono Int16.
- **STT** ([`Sources/STT.swift`](Sources/STT.swift)) — POSTs raw L16 PCM to Google's free
  legacy `speech-api/v2` endpoint as **bn-BD** (correct *Bangladeshi* Bangla — the whole
  reason for the HTTP path; `bn-IN` returns Indian vocabulary). On repeated throttle it falls
  back to `bn-IN`. English uses `en-US`. Unofficial + rate-limited; nothing is stored.
- **Injection** ([`Sources/Inject.swift`](Sources/Inject.swift)) — posts Unicode key events
  (`CGEvent`), control chars stripped, so it types into any focused app.

> **Future upgrade (optional):** English (and Bangla, *if* Apple's recognizer is genuinely
> `bn-BD`) could use the live, on-device `SFSpeechRecognizer` for zero-latency input — the
> doc's preferred English path. v1 uses the uniform HTTP path (only needs Microphone
> permission; no Speech-recognition entitlement). See MAC-PARITY-TODO §1.

## Permissions (granted once, in System Settings → Privacy & Security)
- **Microphone** — to hear you (prompted on first launch).
- **Accessibility** — to type into other apps (menu → "Grant Accessibility…", or add the app
  under Privacy & Security → Accessibility).

The mic is live **only** while a mode is on; no audio or transcript is written or logged; the
only network call is the speech request. Opt-in — you launch the app.

## Build
```bash
./build.sh 1.0.0        # -> dist/Bangla Voice.app  (ad-hoc signed)
```
Needs the Xcode command-line tools (Swift). Unsigned/ad-hoc → first launch via right-click →
Open (or System Settings → Privacy & Security → Open Anyway). Real distribution wants a
Developer ID + notarization (same as the rest of the project).
