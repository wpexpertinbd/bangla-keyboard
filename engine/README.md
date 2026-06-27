# engine/ — the shared reordering engine (source of truth)

[`Engine.swift`](Engine.swift) is the **canonical** Bijoy-style reordering engine: it buffers
one syllable and reorders it so a prebase vowel typed before its consonant lands in correct
Unicode order. It has **no OS dependencies** — pure `(key, shift) -> Result`.

Every platform port (macOS IMK, Windows TSF, Linux IBus) reimplements *this* logic in its own
language and wraps it in a thin OS shell. See [`../SPEC.md`](../SPEC.md) for the language-neutral
description, keymap, algorithm, and test corpus.

**Rule:** if `SPEC.md` and `Engine.swift` ever disagree, `Engine.swift` wins — fix the spec.

This file is a copy of the verified engine from the macOS IME prototype. The macOS *shipping*
product is actually the static `.keylayout` under `../macos/` (a deadkey emulation of this
engine, because Apple gates unsigned IMEs); the engine here is the clean model the other
OSes should follow.
