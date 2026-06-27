# Bangla Keyboard — Windows port (TSF IME)

> **Status: not started.** This folder is the home for the Windows build. Start here.

## Read first
1. [`../SPEC.md`](../SPEC.md) — the complete, OS-neutral engine spec (keymap + algorithm +
   test corpus). **This is the contract.** Your build must reproduce it exactly.
2. [`../engine/Engine.swift`](../engine/Engine.swift) — the 172-line reference engine. Port
   this logic to C++ (or C#). It is the source of truth; if SPEC.md and Engine.swift ever
   disagree, Engine.swift wins.
3. [`../macos/`](../macos/) — the shipping macOS version, for behaviour comparison.

## Why a TSF IME (and not an MSKLC `.klc` layout)
The keyboard's whole point is **reordering one syllable** so a prebase vowel typed *before*
its consonant lands in correct Unicode order (`ে`+`ক`→`কে`), plus reph-after-consonant
(`ভার্সন`) and prebase-through-conjunct (`ক্ষি`). A static Windows layout can do single
deadkeys only — it **cannot** do this. So the real port is a **TSF text service** that runs
the engine. (A `.klc` may ship as a clearly-labelled "basic, no reordering" fallback, but it
is not the deliverable.)

## Build plan (suggested)
1. **Port the engine headless first.** Create `windows/engine/engine.cpp` + `engine.h` as a
   pure module (no Windows headers), then `windows/engine/test.cpp` that feeds the §7 corpus
   and asserts NFC output. Get all tests green before any COM/TSF code.
   - Watch the **hasanta grapheme trap** (test last code unit == U+09CD, not string suffix).
2. **TSF shell.** A 64-bit (and 32-bit) in-proc COM DLL implementing
   `ITfTextInputProcessorEx`, `ITfThreadMgrEventSink`, `ITfKeyEventSink`,
   `ITfCompositionSink`. Map engine results → composition string / commit
   (see SPEC §2 and §8). Bind keys by **scan code** for the US-QWERTY positions in SPEC §6
   so it works under any base layout; flush + pass through on Ctrl/Alt/Win chords.
3. **Register + install.** Register the TIP (CLSID + `bn-BD` language profile) under
   `HKLM\SOFTWARE\Microsoft\CTF\TIP`. Ship an installer (MSIX or a small EXE doing
   `ITfInputProcessorProfiles::Register` + DLL registration). Code-sign to avoid SmartScreen.
4. **Verify** against SPEC §7 on a real Windows box (Notepad + a Unicode app), then compare
   side-by-side with the macOS build.

## Layout this folder like
```
windows/
├── engine/        # pure C++ engine + headless tests (port of ../engine/Engine.swift)
├── tsf/           # the TSF TIP COM DLL (Visual Studio project)
├── installer/     # MSIX / EXE
└── README.md
```

## Don'ts
- No "Bijoy" name anywhere; no bundled commercial fonts (engine emits standard Unicode, so
  no fonts are needed — any Bangla Unicode font renders it).
- Don't hand-port the macOS `.keylayout` deadkey machine — port the **engine**, it's cleaner.
