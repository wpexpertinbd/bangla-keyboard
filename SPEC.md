# Bangla Keyboard — Cross-platform Engine Specification

This is the **single source of truth** for the typing behaviour on every OS.
macOS ships today; Windows and Linux ports must reproduce *exactly* this behaviour.

The reference implementation is [`engine/Engine.swift`](engine/Engine.swift) — 172 lines,
verified on the real macOS `UCKeyTranslate` engine and on a standalone test harness
(types the full national anthem correctly). **Port the algorithm below, not the macOS
keylayout** (the macOS `.keylayout` is a static-deadkey *emulation* of this engine and is
far more convoluted; the engine is the clean model).

---

## 1. What this keyboard is

A **fixed (Bijoy-style) Bangla layout**: each physical key has a fixed Bangla letter/sign,
and an **engine reorders one syllable at a time** so that you can type a **prebase vowel
before its consonant** (the Windows/Bijoy habit) and get correct Unicode order.

Example: typing `ে` then `ক` must produce `কে` (U+0995 U+09C7), not `েক`.

This reordering is the entire value of the product. A plain static layout (Windows MSKLC
`.klc`, or a bare XKB symbols file) **cannot** do it — it needs a small stateful engine:

| OS | Mechanism | Status |
|----|-----------|--------|
| macOS | Static `.keylayout` emulating the engine via chained deadkeys (shipping) **+** an optional IMK IME (blocked by Apple notarization) | ✅ shipping |
| Windows | **TSF text service** (Text Services Framework) implementing this engine | ⬜ to build |
| Linux | **IBus** (and/or Fcitx5) engine implementing this engine | ⬜ later |

> A Windows `.klc` layout is **not** acceptable as the primary deliverable: it would lose
> reph / matra reordering. Build a TSF IME. (A `.klc` MAY ship as a "basic" fallback, clearly
> labelled, but the IME is the real port.)

---

## 2. The architecture decision (one engine, three thin shells)

```
        ┌─────────────────────────────────────────────┐
        │   ENGINE  (this spec / Engine.swift)         │
        │   pure function: (key, shift) -> Result      │
        │   holds one syllable of state; no OS calls   │
        └─────────────────────────────────────────────┘
              ▲                ▲                 ▲
   macOS IMK shell      Windows TSF shell    Linux IBus shell
   (InputController)    (ITfTextInputProc…)  (IBusEngine)
```

The engine has **no OS dependencies** — it takes a key event and returns one of four
results, and the platform shell turns those results into composing/committed text using
that platform's API. Reimplement the engine in the platform's language (C++ for TSF,
C/Vala/Rust/Python for IBus) — it is ~170 lines.

### Engine return type

```
ignore                          // not our key — let the host handle it
compose(text)                   // show `text` as marked/underlined composing text
commit(text)                    // insert `text` as final; clear the buffer
commitThenCompose(final, comp)  // insert `final`, then start a new composing run = `comp`
```

Platform mapping:
- **TSF**: `compose` → set composition string (start composition if none);
  `commit` → end composition with the string; `commitThenCompose` → end composition with
  `final`, immediately start a new composition with `comp`.
- **IBus**: `compose` → `ibus_engine_update_preedit_text` (with underline);
  `commit` → `ibus_engine_commit_text` + clear preedit; `commitThenCompose` → commit `final`
  then update preedit to `comp`.

---

## 3. Buffer model (one syllable)

Three string fields:

| field | holds | examples |
|-------|-------|----------|
| `cons`  | consonant cluster, may end in hasanta `্` mid-cluster | `ক`, `ক্ষ`, `চ্ছ`, `স্ত্র`, `ক্` |
| `vowel` | the (single, combined) vowel sign | `ি`, `ে`, `ো`, `ৌ` |
| `trail` | trailing signs | `ং`, `ঃ`, `ঁ` (may be more than one) |

- **composing (marked) text** = `cons + vowel + trail`.
- **final (committed) text** = same, EXCEPT: if `cons` is empty but `vowel` is set, the lone
  vowel sign becomes its **independent vowel** (`া`→`আ`, `ি`→`ই`, `ে`→`এ`, …). See `indep` map.

`endsWithHasanta(cons)` must be tested on the **last Unicode scalar == U+09CD**, NOT a
string suffix test — in many languages `"ক্"` is a single grapheme cluster and a naive
`endsWith("্")` returns false. (This bit us in Swift; it will bite in C#/C++ too.)

---

## 4. Character categories

```
hasanta      = ্                       (U+09CD)
prebase      = { ি, ে, ৈ }            typed BEFORE their consonant
postbase     = { া, ী, ু, ূ, ৃ, ৗ }   typed after / combine onto the syllable
trailSigns   = { ং, ঃ, ঁ }
independents = { অ, আ, ই, ঈ, উ, ঊ, ঋ, এ, ঐ, ও, ঔ }   complete letters
standalone   = { ৎ }                  khanda-ta: never takes a vowel
clusterMods  = { র্, ্র, ্য }          reph + ra-fola + ya-fola — extend the cluster
consonant    = a single Bengali consonant letter:
               U+0995…U+09B9  OR  U+09DC (ড়)  OR  U+09DD (ঢ়)  OR  U+09DF (য়)

indep map (lone vowel-sign -> independent vowel):
  া→আ  ি→ই  ী→ঈ  ু→উ  ূ→ঊ  ৃ→ঋ  ে→এ  ৈ→ঐ  ো→ও  ৌ→ঔ

vowel combine (when a postbase is added onto an existing vowel):
  ে + া → ো     ে + ৗ → ৌ     (otherwise just concatenate)
```

---

## 5. The algorithm  (`process(key, shift) -> Result`)

Look the key up in the **keymap** (§6) to get a Bangla `unit` string. If unmapped:
- if it's **Backspace** and the buffer is non-empty → `popLast()` (remove last of trail,
  else vowel, else cons), then return `compose(render)` (or `commit("")` if now empty);
- else return `ignore`.

Otherwise, in this order:

1. **`unit == hasanta`** → `cons += unit`; `compose`.
2. **`unit ∈ clusterMods`** (`র্ ্র ্য`) → `cons += unit`; `compose`.
3. **`unit ∈ prebase`** (`ি ে ৈ`):
   - if buffer **not** empty → `final = renderFinal(); reset(); vowel = unit`;
     `commitThenCompose(final, render)`  ← prebase starts a NEW syllable
     (so `হ` then `ে` = commit `হ`, begin `ে…`).
   - else `vowel = unit`; `compose`.
4. **`unit ∈ postbase`** (`া ী ু ূ ৃ ৗ`):
   - if buffer entirely empty → `commit(indep[unit])`  (isolated → independent vowel).
   - else `vowel = combineVowel(vowel, unit)`; `compose`.
5. **`unit ∈ trailSigns`** → `trail += unit`; `compose`.
6. **`unit ∈ independents` OR `unit ∈ standalone`** → `commit(renderFinal() + unit)`; reset.
7. **`unit` is a consonant**:
   - if `cons` empty → `cons = unit`; `compose`  (picks up any pending prebase vowel: `ে`+`ক`=`কে`).
   - else if `endsWithHasanta(cons)` → `cons += unit`; `compose`  (mid-conjunct continues,
     even with a prebase vowel pending: `ি ক ্ ষ` → `ক্ষি`).
   - else → `final = renderFinal(); reset(); cons = unit`;
     `commitThenCompose(final, render)`  (closed cluster → new syllable).
8. **anything else** (digits, punctuation, `।`, `৳`, `—`, etc.) → `commit(renderFinal() + unit)`; reset.

**Flush** (on focus loss, Cmd/Ctrl chord, app switch, Enter where appropriate): emit
`renderFinal()` and reset. The host should also flush before handling a non-text shortcut.

---

## 6. Keymap (OS-neutral, by US-QWERTY physical key)

Bind by **physical key position** (Windows scan code / X keysym for the US-QWERTY position),
NOT by the character the host OS would produce — the layout overrides 26 letters + digits.

`·` = key produces itself (passthrough). Blank shift cell = same as unshifted unless noted.

### Letters / main keys

| QWERTY key | unshifted | shift |
|---|---|---|
| `` ` `` | — (em dash) | ‚ |
| `q` | ঙ | ং |
| `w` | য | য় |
| `e` | ড | ঢ |
| `r` | প | ফ |
| `t` | ট | ঠ |
| `y` | চ | ছ |
| `u` | জ | ঝ |
| `i` | হ | ঞ |
| `o` | গ | ঘ |
| `p` | ড় | ঢ় |
| `[` | · `[` | { |
| `]` | · `]` | } |
| `a` | ৃ | র্  (reph) |
| `s` | ু | ূ |
| `d` | ি | ী |
| `f` | া | অ |
| `g` | ্  (hasanta) | ।  (danda) |
| `h` | ব | ভ |
| `j` | ক | খ |
| `k` | ত | থ |
| `l` | দ | ধ |
| `;` | · `;` | : |
| `'` | · `'` | " |
| `\` | ৎ  (khanda-ta) | ঃ |
| `z` | ্র  (ra-fola) | ্য  (ya-fola) |
| `x` | ও | ৗ  (au length mark) |
| `c` | ে | ৈ |
| `v` | র | ল |
| `b` | ন | ণ |
| `n` | স | ষ |
| `m` | ম | শ |
| `,` | · `,` | < |
| `.` | · `.` | > |
| `/` | · `/` | ? |

### Digits row

| key | unshifted | shift |
|---|---|---|
| `1` | ১ | ! |
| `2` | ২ | ° |
| `3` | ৩ | # |
| `4` | ৪ | ৳ (taka) |
| `5` | ৫ | % |
| `6` | ৬ | ^ |
| `7` | ৭ | ঁ |
| `8` | ৮ | * |
| `9` | ৯ | ( |
| `0` | ০ | ) |
| `-` | · `-` | _ |
| `=` | · `=` | + |

> Source of truth = the `map` in `engine/Engine.swift` (keyed by macOS virtual key codes).
> The table above is that same map translated to US-QWERTY positions. If they ever disagree,
> **Engine.swift wins** and this table is the bug.

---

## 7. Test corpus (must all pass)

Type the keys (Windows order — prebase vowel typed before its consonant where natural) and
compare **NFC-normalized** output. These are the regression cases the macOS engine passes:

| word | key sequence (QWERTY, ⇧=shift) | expected |
|---|---|---|
| কে | `c j` | কে |
| কি | `d j` | কি |
| কো | `c j f`  (ে ক া → combine) | কো |
| কৈ | `⇧c j` | কৈ |
| ক্ষি | `d j g ⇧n` (ি ক ্ ষ) | ক্ষি |
| হচ্ছে | `i y g ⇧y c`? use engine order | হচ্ছে |
| ভার্সন | `⇧h f n ⇧a b` (ভ া স র্ ন — **reph after consonant**) | ভার্সন |
| কর্ম | `j m ⇧a` (ক ম র্) | কর্ম |
| ফ্রি | `d r z`? (ি ফ ্র) | ফ্রি |
| সোনার | `n c x?`… (স ো ন া র) | সোনার |
| বাংলা | `h f ⇧q v f` | বাংলা |
| আমি | `⇧f f m d` (অ া ম ি) | আমি |
| ভাই | `⇧h f g d` (ভ া ্ ি → ই) | ভাই |
| বল | `h ⇧v` | বল |

> The two non-negotiable, historically-fragile cases are **reph-after-consonant** (`ভার্সন`,
> `কর্ম`) and **prebase-vowel-through-conjunct** (`ক্ষি`). Test these first.

Use the macOS ground-truth harness pattern (`UCKeyTranslate`) to capture authoritative
expected output for any new word; or run `engine/Engine.swift` directly in a Swift test
file (see §9).

---

## 8. Per-platform shell notes

### Windows (TSF text service)
- Implement `ITfTextInputProcessor`, `ITfThreadMgrEventSink`, `ITfKeyEventSink`,
  `ITfCompositionSink`. Register a TIP with a CLSID + a language profile for `bn-BD`.
- Map the engine results: composition = `ITfComposition` with display attribute (underline);
  commit = end composition inserting text; commitThenCompose = end then start a new one.
- Bind keys by **scan code** for the US-QWERTY positions in §6 (so it works regardless of
  the user's base layout). Pass through Ctrl/Alt/Win chords (flush first).
- Ship a signed installer: TSF TIPs must be a 64-bit (and ideally 32-bit) in-proc COM DLL,
  registered under `HKLM\…\CTF\TIP`. A `.msix` or a simple `regsvr32` + Text-Services
  registration installer. Code-signing strongly recommended (SmartScreen).
- Reuse `windows/` for the VS solution; keep the engine in its own static lib/`.cpp` so it
  can be unit-tested headless.

### Linux (IBus, later)
- Implement an `IBusEngine` subclass; the engine maps cleanly to
  `update_preedit_text` / `commit_text`. Package as an IBus component + `.xml`.
- Bind by keysym for the US-QWERTY positions. Fcitx5 addon optional, same engine.
- Note: **OpenBangla Keyboard** already exists for Linux (Avro + fixed layouts). We are NOT
  forking it — but it's a useful reference for IBus/Fcitx packaging.

---

## 9. Testing the engine in isolation

The macOS engine can be exercised without any IME plumbing:
```bash
# in BanglaKeyboardIME/, compile Engine.swift with a tiny main.swift of test code:
swiftc Sources/Engine.swift /tmp/test_main.swift -o /tmp/enginetest && /tmp/enginetest
```
For Windows/Linux: write the engine as a pure module + a console test that feeds key
sequences and prints NFC output; assert against §7. Do this BEFORE wiring the OS shell.

---

## 10. License / branding

MIT (see `LICENSE`). De-branded "Bangla Keyboard" — **no "Bijoy" trademark anywhere**, no
proprietary fonts in the repo (macOS Classic needs user-supplied SutonnyMJ; the Unicode
layout + all three OS IMEs are font-independent — they emit standard Unicode). Keep every
new file free of the Bijoy name and of any commercial font.
