import json, glob, os, collections

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")

def load_lines(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for ln in f:
            ln = ln.strip()
            if not ln:
                continue
            try:
                out.append(json.loads(ln))
            except Exception:
                pass
    return out

def hist(path, label):
    objs = load_lines(path)
    c = collections.Counter(o[0] if isinstance(o, list) and o else type(o).__name__ for o in objs)
    print("=" * 90)
    print(label, path)
    for k, v in c.most_common():
        print("   %-16s %d" % (k, v))
    return objs

ep = hist(glob.glob(ROOT + r"\PCB\*.epcb")[0], "PCB")
es = hist(glob.glob(ROOT + r"\SHEET\*\*.esch")[0], "SCH")

print()
print("--- PCB: samples of pad / net / component / other structural records ---")
seen = set()
for o in ep:
    t = o[0]
    if t in ("LAYER", "HEAD", "CANVAS", "DOCTYPE", "LINESTYLE", "FONTSTYLE", "LAYER_STYLE", "GRID", "UNIT"):
        continue
    if t in seen:
        continue
    seen.add(t)
    print(t, "::", json.dumps(o)[:400])

print()
print("--- SCH: samples of non-trivial records ---")
seen = set()
for o in es:
    t = o[0]
    if t in ("FONTSTYLE", "LINESTYLE", "HEAD", "DOCTYPE", "CANVAS"):
        continue
    if t in seen:
        continue
    seen.add(t)
    print(t, "::", json.dumps(o)[:400])
