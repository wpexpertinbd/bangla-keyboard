# Bangla Keyboard — Linux port (IBus)

> **Status: shipping — `linux-v1.1.1`.** An **IBus engine** that reuses the shared C++ **KLEngine** (the
> same keylayout-driven FSM as macOS + Windows), so output is **byte-identical**
> (`f`→া, `Shift+f`→অ, `Shift+f f`→আ, reph reorders, conjuncts). Two engines:
> **Bangla Unicode** + **Bangla Classic**. Built + tested on **Ubuntu 24.04** (GNOME/
> IBus); should work on any distro with IBus (Debian, Fedora, Arch, …).

Verified end-to-end: real key events → IBus → correct Bangla (`linux/test.sh`, 5/5),
plus the shared engine's own corpus (20/20 Unicode + 12/12 Classic) on Linux.

## How it works
- **One engine, reused.** `ibus-bangla.cpp` is a thin IBus `IBusEngine` subclass; the
  actual Bangla logic is the shared [`../windows/engine`](../windows/engine) `KLEngine`
  (pure C++, no OS deps) — so there is **one** engine across all three platforms.
- **Keys.** IBus gives the X11 keycode; a Linux **evdev keycode equals the Windows
  Set-1 scan code** the tables use (both come from the IBM PC/AT set), and X11 keycode
  = evdev + 8 — so `scan = keycode - 8`, no mapping table.
- **Preedit/commit.** The in-progress syllable lives in the **preedit** (underlined);
  it commits on a word boundary / Ctrl-Alt chord / focus-out. This is cleaner than the
  Windows tray's back-spacing — IBus redraws the whole preedit, so prebase-vowel and
  reph **reordering just works**. All Ctrl/Alt/Super shortcuts pass through untouched.

## Requirements
```sh
# Debian/Ubuntu — keyboard + voice typing:
sudo apt install build-essential pkg-config libibus-1.0-dev ibus \
                 libpulse-dev libcurl4-openssl-dev libnotify-bin
# Fedora:  sudo dnf install gcc-c++ pkgconf-pkg-config ibus-devel ibus \
#                           pulseaudio-libs-devel libcurl-devel libnotify
# Arch:    sudo pacman -S base-devel ibus libpulse curl libnotify
```
(`libpulse` + `libcurl` are for **voice typing**; `libnotify` shows the "voice on"
toast. Voice is optional — the keyboard builds/works without a mic.)

## Install — Debian / Ubuntu (`.deb`, easiest)
Download `bangla-keyboard-ibus_<ver>_amd64.deb` from the
[**latest release**](https://github.com/wpexpertinbd/bangla-keyboard/releases), then:
```sh
sudo apt install ./bangla-keyboard-ibus_*.deb   # pulls in ibus if needed
```
**Log out and back in** (GNOME reads new IBus engines at login), then add the input
source (below). Build your own `.deb` with `./build-deb.sh`.

## Build & install from source (any IBus distro)
```sh
cd linux
./build.sh              # -> linux/dist/ibus-engine-bangla (+ self-test 20/20, 12/12)
sudo ./install.sh       # installs the binary + IBus component, system-wide
ibus restart            # or log out / back in
```
Then add it as an input source:
- **GNOME:** Settings → Keyboard → Input Sources → **+** → **Bangla** →
  *Bangla Unicode* (and/or *Bangla Classic*).
- Switch inputs with **Super+Space**; type on a US-QWERTY layout.

`./test.sh` runs the end-to-end key→commit test (needs the engine installed).
`sudo ./uninstall.sh` removes it.

## Typing
Fixed Windows-style layout with syllable reordering — type a prebase vowel **before**
its consonant and it reorders (`ে`+`ক`→`কে`); reph after a consonant reorders
(`ভার্সন`); independent vowels: `f`→া, `Shift+f`→অ, `Shift+f` then `f`→আ.

**Bangla Classic** outputs the legacy ASCII (non-Unicode) encoding of legacy ANSI
("MJ"-style) Bangla fonts. Those fonts are proprietary and **not** included — install a
compatible legacy ANSI Bangla font from your own legitimate source and select it in
your app. **Bangla Unicode** works with any Bangla Unicode font (most distros ship one;
e.g. `sudo apt install fonts-beng`).

## Voice typing (optional, needs internet)
While a Bangla engine is active, press **Ctrl+Alt+S** (Bangla) or **Ctrl+Alt+D**
(English), speak, and the recognized text is typed at the cursor (committed through
IBus, so it works on X11 **and** Wayland). Press again to stop; a toast shows the state.
- **Free, nothing stored** — English + Bangla go to the same free online speech service
  the Windows/macOS builds use (native `libcurl`, no browser); correct Bangladeshi `bn-BD`.
- **Punctuation is spoken** (STT gives none): say the mark ALONE after a pause —
  "দাঁড়ি"→।, "কমা"→,, "প্রশ্ন"→?, "বিস্ময়"→! (English: comma / period / question mark /
  exclamation mark). A word inside a sentence stays a word.
- Needs a working microphone (PulseAudio/PipeWire) + internet. It's an inherent part of
  the engine; nothing else to install beyond the runtime libs above.

## Files
- `ibus-bangla.cpp` — the IBus engine (reuses `../windows/engine` KLEngine) + voice.
- `stt_curl.h` — free online STT over libcurl (Bangla `bn-BD` + English).
- `ibus-selftest.cpp` — end-to-end key→commit test client.
- `build.sh` / `install.sh` / `uninstall.sh` / `test.sh`.
- `dist/` — build output (git-ignored).

## Notes
- **Wayland & X11** both work (IBus handles both). Under Wayland, IBus is the standard
  input-method path for GTK/Qt apps.
- Not building a Fcitx5 addon yet — IBus covers GNOME (the most common default) and is
  available everywhere. Fcitx5 can be added later reusing the same KLEngine.
- Reference only (not forked): **OpenBangla Keyboard** — a mature Bangla IBus/Fcitx
  project, useful as a packaging example.
