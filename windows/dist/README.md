# Bangla Keyboard — Windows build artifacts

Built by [`../build-all.bat`](../build-all.bat) / [`../build-all.sh`](../build-all.sh).
The binaries themselves are git-ignored (rebuild them locally); this folder holds:

| artifact | what it is | how to use |
|---|---|---|
| `enginetest.exe` | the SPEC §7 corpus test | double-click / run → expect **13/13 passed** |
| **`bangla-tray.exe`** | **Bijoy-style tray switcher** — the easiest way to actually use it | double-click → a tray icon appears. **Ctrl+Alt+B** or click the icon to switch Bangla⇄English, then type in **any** app. Right-click → Close. No admin, no install. See [`../tray/`](../tray/) |
| `bangla-demo.exe` | standalone typing demo (not the IME) | run it, type on your US-QWERTY keyboard, watch live Bangla. `[ ]` marks the composing syllable. **Enter** = new line, **Esc** = quit. Batch mode: `bangla-demo.exe --keys "c j f"` → `কো` |
| `BanglaKeyboard.dll` | **the IME (x64)** — a TSF text service for 64-bit apps | register it (below), then switch to it with **Win+Space** in any app |
| `BanglaKeyboard32.dll` | **the IME (x86)** — same thing for 32-bit apps (loaded under WOW64) | registered alongside the x64 one; same CLSID |
| `loadtest.exe` / `loadtest32.exe` | non-destructive DLL checks (x64 / x86) | `loadtest.exe BanglaKeyboard.dll` and `loadtest32.exe BanglaKeyboard32.dll` → load the DLL, create the COM object, QI the TSF sinks. **No registry changes.** Expect **LOADTEST PASS** |

## Install the IME (needs Administrator)
```
..\installer\register.bat       (right-click → Run as administrator)
```
Then add it: **Settings → Time & language → Language & region → Bangla →
Language options → add a keyboard → "Bangla Keyboard (Unicode)"**, or switch with
**Win+Space**. Remove with `..\installer\unregister.bat` (as admin).

## Status / caveats
- ✅ Engine: **13/13** SPEC corpus (compiled `enginetest.exe`).
- ✅ Both DLLs (**x64 + x86**) build, load, and their COM/TSF vtables verify
  (`loadtest.exe` / `loadtest32.exe`).
- ⬜ **Live in-app typing not yet verified** — that needs registering the IME on a
  real Windows session and typing in Notepad/Word. Do that to fully sign it off.
- ⬜ Composing-text **underline** (display attribute) is still a TODO — composition
  works without it; it's cosmetic. See `../tsf/README.md`.
- ⬜ **Unsigned.** Code-sign before distributing (SmartScreen). See `../installer/`.
