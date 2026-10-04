"""Bangla Phonetic parity check for the ported C++ engine (KLEngine + phonetic_table.h).

Two checks, both NFC-compared:
  1. CORPUS   — macos/src/phonetic/tests.tsv (the shipped regression corpus).
  2. DIFF     — every corpus prefix (= what the live preview shows mid-word) plus N
                random typings, against type_ref() in macos/src/phonetic/gen_phonetic.py
                (the reference model the macOS layout itself is generated from).

Usage:  python phonetic_check.py <path-to-phonetic_driver(.exe)> [nrandom]
"""
import importlib.util
import os
import random
import subprocess
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
PHON = os.path.join(HERE, "..", "..", "macos", "src", "phonetic")
NFC = lambda s: unicodedata.normalize("NFC", s)


def load_ref():
    spec = importlib.util.spec_from_file_location("genphon", os.path.join(PHON, "gen_phonetic.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def run_driver(driver, cases):
    """Feed one typed case per line; get one output line back."""
    p = subprocess.run([driver], input="\n".join(cases) + "\n",
                       capture_output=True, encoding="utf-8")
    if p.returncode != 0:
        sys.exit("driver failed (%d): %s" % (p.returncode, p.stderr[:400]))
    out = p.stdout.split("\n")
    return out[:len(cases)]


def main():
    driver = sys.argv[1]
    nrand = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
    ref = load_ref()

    corpus = []
    for line in open(os.path.join(PHON, "tests.tsv"), encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        typed, expected = line.split("\t")[:2]
        corpus.append((typed, expected))

    # ---- 1. shipped corpus -------------------------------------------------
    got = run_driver(driver, [t for t, _ in corpus])
    bad = [(t, e, g) for (t, e), g in zip(corpus, got) if NFC(g) != NFC(e)]
    print("corpus : %d/%d" % (len(corpus) - len(bad), len(corpus)))
    for t, e, g in bad[:15]:
        print("   MISMATCH %-16s expected %-22s got %s" % (t, e, g))

    # ---- 2. differential vs the reference model ----------------------------
    cases = set()
    for typed, _ in corpus:
        for k in range(1, len(typed) + 1):
            cases.add(typed[:k])                      # every prefix = a live-preview state
    # Every letter in BOTH cases (not just the ones the corpus happens to use) so the
    # "Shift on a case-insensitive letter == lowercase" rule is exercised for all of
    # them — that path is a keymap[1] miss falling back to keymap[0] in KLEngine.
    alphabet = sorted({c for typed, _ in corpus for c in typed}
                      | set("abcdefghijklmnopqrstuvwxyz")
                      | set("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
                      | set("`,.:^$0123456789"))
    rnd = random.Random(1234)
    while len(cases) < nrand:
        cases.add("".join(rnd.choice(alphabet) for _ in range(rnd.randint(1, 12))))
    cases = sorted(cases)

    got = run_driver(driver, cases)
    bad = [(c, ref.type_ref(c), g) for c, g in zip(cases, got) if NFC(g) != NFC(ref.type_ref(c))]
    print("diff   : %d/%d  (corpus prefixes + random, vs gen_phonetic.type_ref)"
          % (len(cases) - len(bad), len(cases)))
    for c, e, g in bad[:15]:
        print("   MISMATCH %-16s ref %-22s got %s" % (c, e, g))

    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
