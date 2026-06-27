# Bangla Keyboard — Linux port (IBus / Fcitx5)

> **Status: planned (after Windows).** Placeholder.

The same engine ([`../SPEC.md`](../SPEC.md), [`../engine/Engine.swift`](../engine/Engine.swift))
ported to an **IBus engine** (and optionally a Fcitx5 addon). The engine's four results map
directly onto IBus:
- `compose` → `ibus_engine_update_preedit_text` (with underline)
- `commit`  → `ibus_engine_commit_text` (+ clear preedit)
- `commitThenCompose` → commit, then update preedit

Bind keys by keysym for the US-QWERTY positions in SPEC §6. Package as an IBus component
(`/usr/share/ibus/component/*.xml`) + the engine binary/script. Language: C, Vala, Rust, or
Python (`pygobject`) — pick whatever keeps the engine a clean, testable module.

Reference (NOT to fork): **OpenBangla Keyboard** already does Bangla on Linux — useful only
as a packaging/IBus example.

Build the headless engine + §7 tests first, then wire the IBus shell.
