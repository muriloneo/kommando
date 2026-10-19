import json, os, glob

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")

def load(p):
    out = []
    for ln in open(p, encoding="utf-8"):
        ln = ln.strip()
        if ln:
            try:
                out.append(json.loads(ln))
            except Exception:
                pass
    return out

for uuid, label in (("7676aead2dfc4e839020fd4f7b51e58b", "U1 = ESP32-C6 module"),
                    ("a21dd8ab4739", "U4 = screen connector")):
    hits = glob.glob(ROOT + r"\SYMBOL\\" + uuid[:12] + "*.esym")
    print("=" * 80)
    print(label, hits)
    if not hits:
        continue
    rows = []
    for o in load(hits[0]):
        if isinstance(o, list) and o and o[0] == "PIN":
            rows.append((str(o[2]), str(o[3])))
    print("pins:", len(rows))
    for num, name in rows:
        print("   pad %-5s name=%s" % (num, name))
