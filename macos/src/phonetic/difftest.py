"""Differential test: real macOS engine vs the Python reference, on corpus prefixes + random input."""
import sys, random, unicodedata, importlib.util
sys.argv_saved = sys.argv
spec = importlib.util.spec_from_file_location("gt", __import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)), "gt.py")); gt = importlib.util.module_from_spec(spec); spec.loader.exec_module(gt)
spec = importlib.util.spec_from_file_location("g", sys.argv[1]); g = importlib.util.module_from_spec(spec); spec.loader.exec_module(g)
layout, corpus, nrand = sys.argv[2], sys.argv[3], int(sys.argv[4])
u = gt.load(layout); assert u is not None, "load fail"
N = lambda s: unicodedata.normalize("NFC", s)
cases = set()
for line in open(corpus, encoding="utf-8"):
    line = line.rstrip("\n")
    if not line or line.startswith("#"): continue
    typed = line.split("\t")[0]
    for k in range(1, len(typed) + 1): cases.add(typed[:k])            # every prefix = live preview
rnd = random.Random(1234)
alpha = [c for c in g.ACTION_CHARS] + list("aeiouokntrsmbdlpgh") * 3 + [" "] * 6 + list("0123456789$^?!;")
for _ in range(nrand):
    cases.add("".join(rnd.choice(alpha) for _ in range(rnd.randint(1, 14))))
bad = 0
for typed in sorted(cases):
    t = typed if typed.endswith(" ") else typed + " "
    got, _ = gt.type_text(u, t)
    want = g.type_ref(t)
    if N(got) != N(want):
        bad += 1
        if bad <= 15: print("MISMATCH %r: engine %r  ref %r" % (typed, got, want))
print("%d cases, %d mismatches" % (len(cases), bad))
sys.exit(1 if bad else 0)                # non-zero on any mismatch: usable as a release gate
