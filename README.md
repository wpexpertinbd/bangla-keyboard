<h1 align="center">Bangla Keyboard</h1>

<p align="center">
  A free, open-source <b>Bijoy-style fixed Bangla keyboard</b> for <b>macOS, Windows, and Linux</b>.<br>
  Type a prebase vowel before its consonant (the Windows/Bijoy habit) and get correct Unicode order.<br>
  No "Bijoy" branding, no proprietary fonts — emits standard Unicode.
</p>

<p align="center"><i>Built and maintained by <a href="https://www.biswashost.com">BiswasHost</a> 🇧🇩</i></p>

---

## Platforms

| OS | What it is | Status | Folder |
|----|-----------|--------|--------|
| 🍎 **macOS** | Native `.keylayout` (Unicode + Classic) + smart installer `.pkg`/`.dmg` | ✅ **shipping** | [`macos/`](macos/) |
| 🪟 **Windows** | TSF IME (Text Services Framework) running the shared engine | ⬜ in progress | [`windows/`](windows/) |
| 🐧 **Linux** | IBus / Fcitx5 engine running the shared engine | ⬜ planned | [`linux/`](linux/) |

**macOS users:** grab the latest `.pkg`/`.dmg` from [**Releases**](../../releases) and see
[`macos/README.md`](macos/README.md).

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
├── windows/         # Windows TSF IME (in progress)
├── linux/           # Linux IBus/Fcitx engine (planned)
├── LICENSE          # MIT
├── DISCLAIMER.md    # not affiliated with any commercial keyboard/font vendor
└── SECURITY.md
```

## License
MIT — see [`LICENSE`](LICENSE). De-branded; not affiliated with any commercial Bangla
keyboard or font vendor — see [`DISCLAIMER.md`](DISCLAIMER.md).

<p align="center">Made with care by <a href="https://www.biswashost.com">BiswasHost</a> 🇧🇩</p>
