# Bangla Keyboard — tray switcher (Bijoy-style)

`bangla-tray.exe` is a standalone background app with a **system-tray icon + popup
menu** (Unicode / Bijoy Classic / English) — the same switch-from-the-tray UX as
classic Bijoy. It does **not** need the TSF IME, registration, or admin: just run
the .exe.

## How it works
- A green **অ** tray icon = Unicode (Bangla) mode; a red **E** = English.
- Switch with: **left-click the tray icon**, the **right-click menu**, or **Ctrl+Alt+B**.
- In Unicode mode a global low-level keyboard hook (`WH_KEYBOARD_LL`) runs each
  keystroke through the shared engine ([`../engine/`](../engine/)) and injects the
  resulting Bangla with `SendInput`, so it works in **any** app (Notepad, Word,
  browsers, chat). English mode passes every key straight through.
- The layout + reordering are identical to the rest of the project (it's the same
  engine): prebase vowel before consonant, reph-after-consonant, conjuncts, etc.

## Run it
1. Build (see [`../build-all.bat`](../build-all.bat)) → `../dist/bangla-tray.exe`.
2. Double-click it. A tray icon appears (starts in **English**).
3. Press **Ctrl+Alt+B** (or click the icon) to switch to **Bangla**, then type.
4. Right-click the icon → **Close** to quit.

To start it automatically at login, drop a shortcut to `bangla-tray.exe` in
`shell:startup` (Win+R → `shell:startup`).

## Tray vs IME — which one?
| | tray app (`bangla-tray.exe`) | TSF IME (`../tsf/`) |
|---|---|---|
| Install | none — just run | `regsvr32` (admin), add keyboard |
| Switch | tray icon / Ctrl+Alt+B | Win+Space (Windows switcher) |
| Works in | every app (global hook + inject) | every TSF-aware app (composition) |
| Robustness | edits text by injecting backspaces — can be off in apps with grapheme-cluster backspace, password boxes, or some games | proper IME composition; the "correct" Windows way |
| Feel | exactly like classic Bijoy | like a built-in Windows language |

Both share the **same engine**, so typing behaviour matches. Use the tray app for
the Bijoy-style experience; use the IME for the most robust integration.

## Limitations / TODO
- **Bijoy Classic** menu item is a stub (greyed). Classic = ASCII/SutonnyMJ output,
  which is font-dependent; the engine here emits Unicode only.
- Injection (backspace-diff) can misbehave in apps that delete by grapheme cluster,
  in password fields, or in some full-screen games. The TSF IME avoids this.
- x64 only — a global hook works across all apps regardless of their bitness, so no
  separate 32-bit build is needed.
- Unsigned — code-sign before distributing (a keyboard hook + unsigned exe will
  draw SmartScreen / AV attention).
