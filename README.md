<h1 align="center">Bangla Keyboard</h1>

<p align="center">
  A free, open-source <b>fixed-layout Bangla keyboard</b> for <b>macOS, Windows, and Linux</b> — plus a <b>phonetic</b> layout on macOS.<br>
  Type a prebase vowel before its consonant (the Windows-order habit) and get correct Unicode order.<br>
  No trademarked branding, no proprietary fonts — emits standard Unicode.
</p>

<p align="center"><i>Built and maintained by <a href="https://www.biswashost.com">BiswasHost</a> 🇧🇩</i></p>

---

## Platforms

| OS | What it is | Status | Folder |
|----|-----------|--------|--------|
| 🍎 **macOS** | Native `.keylayout` (Unicode + Classic + **Phonetic**) + smart installer `.pkg`/`.dmg`; **voice typing** companion app (spoken punctuation) | ✅ **v1.7.0** | [`macos/`](macos/) |
| 🪟 **Windows** | Tray app (Bangla Unicode + Classic + **Phonetic** + **voice typing**) running the shared engine, + a TSF IME | ✅ **v1.2.1** | [`windows/`](windows/) |
| 🐧 **Linux** | IBus engine (Bangla Unicode + Classic + **Phonetic** + **voice typing**) running the shared engine | ✅ **v1.2.0** (Debian/Ubuntu + any IBus distro) | [`linux/`](linux/) |

## Install & use

Grab your platform's build from the [**releases page**](https://github.com/wpexpertinbd/bangla-keyboard/releases), then:

### 🍎 macOS
1. Download **`Bangla Keyboard.pkg`** from the [latest release](https://github.com/wpexpertinbd/bangla-keyboard/releases/latest) → double-click → **Done** → **System Settings → Privacy & Security → Open Anyway** (unsigned build).
2. Log out/in, then **System Settings → Keyboard → Text Input → Edit → `+` → Bangla** → add **Bangla Unicode**, **Classic** and/or **Phonetic** (type by sound: `ami` → আমি). Switch with **⌃Space**.
3. **Voice** (optional): download **`Bangla-Voice-macOS-*.zip`**, open the app, allow Accessibility → **⌃⌥S** Bangla · **⌃⌥D** English.

More: [`macos/README.md`](macos/README.md).

### 🪟 Windows
1. Download **`BanglaKeyboard-Setup-*.exe`** (Windows release) from the [releases page](https://github.com/wpexpertinbd/bangla-keyboard/releases) → run it (**per-user, no admin**; SmartScreen → *More info → Run anyway*).
2. A **tray icon** appears — pick **Bangla Unicode / Classic / Phonetic / English**. Shortcuts: **Ctrl+Alt+V** Unicode · **Ctrl+Alt+B** Classic · **Ctrl+Alt+P** Phonetic (each toggles back to English). Type in any app.
3. **Voice** (opt-in at install): **Ctrl+Alt+S** Bangla · **Ctrl+Alt+D** English.

More: [`windows/README.md`](windows/README.md).

### 🐧 Linux (Debian / Ubuntu + any IBus distro)
1. Download **`bangla-keyboard-ibus_*.deb`** (Linux release) from the [releases page](https://github.com/wpexpertinbd/bangla-keyboard/releases): `sudo apt install ./bangla-keyboard-ibus_*.deb`  (other distros, or building from source on a fresh machine: [`linux/README.md`](linux/README.md) has a single line that installs the tools, clones and builds).
2. Log out/in, then **Settings → Keyboard → Input Sources → `+` → Bangla → Bangla (Unicode)** (and/or **Classic** / **Phonetic**). Switch with **Super+Space**.
3. **Voice**: **Ctrl+Alt+S** Bangla · **Ctrl+Alt+D** English.

More: [`linux/README.md`](linux/README.md).

> **Typing:** on the **fixed** layouts (Unicode/Classic) type a prebase vowel *before* its consonant and it reorders (`ে`+`ক`→`কে`, `ভ া স র্ ন`→`ভার্সন`). **Bangla Phonetic** instead types Bangla the way it **sounds** — `ami`→আমি, `hyalO`→হ্যালো, `TesT`→টেস্ট, `dhonyobad`→ধন্যবাদ — with Bangla appearing as you type (case matters: `t/T`, `d/D`, `o/O`; Caps Lock alone is harmless). **[Phonetic cheat sheet →](docs/PHONETIC.md)** (rules, everyday words, and what to type when you get the wrong one). **Voice** needs a microphone + internet, is **free with nothing stored**, and you **speak the punctuation** — say the mark alone after a pause: "দাঁড়ি"→। , "কমা"→, , "প্রশ্ন"→? , "বিস্ময়"→!. **Bangla Classic** needs a legacy ANSI ("MJ"-style) font, not included; **Bangla Unicode** works with any Unicode Bangla font.

## How it works — one engine, three thin shells

The value of this keyboard is **syllable reordering** (so `ে`+`ক`→`কে`, `ভ া স র্ ন`→`ভার্সন`,
`ি ক ্ ষ`→`ক্ষি`). A static OS layout can't do that; it needs a tiny stateful engine. So all
three platforms share **one engine** and wrap it in a thin OS-specific shell:

- **[`SPEC.md`](SPEC.md)** — the OS-neutral specification: keymap, algorithm, and the test
  corpus every port must pass. **The contract.**
- **[`engine/`](engine/)** — the canonical reference engine (`Engine.swift`, ~170 lines, verified).
  Each port reimplements this in its platform's language.

> Porting to a new OS? Read [`SPEC.md`](SPEC.md), port [`engine/Engine.swift`](engine/Engine.swift)
> headless first, pass the §7 tests, then wire the OS shell. Don't hand-port the macOS
> deadkey `.keylayout` — the engine is the clean model.

## Repository layout
```
.
├── SPEC.md          # shared engine spec — the contract for all ports
├── engine/          # canonical reference engine (Engine.swift) + notes
├── macos/           # shipping macOS build (.keylayout + installer)
├── windows/         # Windows tray app (Unicode + Classic + Phonetic) + voice typing
├── linux/           # Linux IBus engine (Unicode + Classic + Phonetic + voice)
├── LICENSE          # MIT
├── DISCLAIMER.md    # not affiliated with any commercial keyboard/font vendor
└── SECURITY.md
```

## License
MIT — see [`LICENSE`](LICENSE). De-branded; not affiliated with any commercial Bangla
keyboard or font vendor — see [`DISCLAIMER.md`](DISCLAIMER.md).

## ☕ Support

This project is free and open-source, on macOS, Windows, and Linux. If it made your
Bangla typing easier, you can **buy me a coffee** — it genuinely helps me keep building
and maintaining free tools like this. 🙏

- **bKash/Nagad** (Personal · *Send Money*): **`01710378396`**

ধন্যবাদ! / Thank you!

<p align="center">Made with care by <a href="https://www.biswashost.com">BiswasHost</a> 🇧🇩</p>
