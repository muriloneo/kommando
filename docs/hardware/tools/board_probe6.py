import json, glob, os, collections

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")

def load(path):
    out = []
    for ln in open(path, encoding="utf-8"):
        ln = ln.strip()
        if ln:
            try:
                out.append(json.loads(ln))
            except Exception:
                pass
    return out

def dump(path, want, label):
    objs = load(path)
    print("=" * 95)
    print(label, os.path.basename(path), dict(collections.Counter(o[0] for o in objs if isinstance(o, list) and o)))
    for o in objs:
        if o[0] in want:
            print(json.dumps(o)[:300])
    return objs

p = ROOT + r"\FOOTPRINT\cd1048b715704363a29af62dff3e951f.efoo"
dump(p, {"PAD", "ATTR", "COMPONENT", "FONTSTYLE"} if False else {"ATTR"}, "U1 FOOTPRINT attrs")

print()
objs = load(p)
pads = [o for o in objs if o[0] == "PAD"]
print("PAD count:", len(pads))
for o in pads[:6] + pads[-4:]:
    print(json.dumps(o)[:340])

print()
print("### esym candidates")
for name in ("4128d35b1a1f4740ad2b75a01e5c0502.esym", "7676aead2dfc4e839020fd4f7b51e58b.esym"):
    objs = load(ROOT + r"\SYMBOL\\" + name)
    print("=" * 95)
    print(name, dict(collections.Counter(o[0] for o in objs if isinstance(o, list) and o)))
    for o in objs:
        if o[0] in ("ATTR",) and len(o) > 4:
            print("ATTR", json.dumps(o)[:200])
    pins = [o for o in objs if o[0] == "PIN"]
    print("PIN count:", len(pins))
    for o in pins[:8]:
        print(json.dumps(o)[:260])
