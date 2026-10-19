import json, glob, os, collections, re

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")

def load(path):
    out = []
    for ln in open(path, encoding="utf-8"):
        ln = ln.strip()
        if not ln:
            continue
        try:
            out.append(json.loads(ln))
        except Exception:
            pass
    return out

# Which files mention the U1 footprint/device uuids or ESP32?
for pat in ("cd1048b71570", "d2cfadff248c", "ESP32"):
    hits = []
    for p in glob.glob(ROOT + r"\**\*.e*oo", recursive=True) + glob.glob(ROOT + r"\**\*.esym", recursive=True):
        try:
            t = open(p, encoding="utf-8", errors="ignore").read()
        except Exception:
            continue
        if pat.lower() in t.lower():
            hits.append((os.path.basename(p), len(t)))
    print(pat, "->", hits[:8])

print()
fp = glob.glob(ROOT + r"\FOOTPRINT\cd1048b71570.efoo")
if fp:
    objs = load(fp[0])
    print("=== U1 footprint", os.path.basename(fp[0]), "records:", dict(collections.Counter(o[0] for o in objs if isinstance(o, list))))
    for o in objs:
        if o[0] in ("PAD", "ATTR", "PAD_NET") and len(o) > 4:
            print(json.dumps(o)[:260])
