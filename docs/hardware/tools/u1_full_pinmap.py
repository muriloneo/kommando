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

# U1 symbol: pair NUMBER and NAME attrs by their parent pin element id
sym = load(glob.glob(ROOT + r"\SYMBOL\7676aead2dfc*.esym")[0])
num, nam, symname = {}, {}, None
for o in sym:
    if not (isinstance(o, list) and o and o[0] == "ATTR") or len(o) < 5:
        continue
    if o[2] == "" and o[3] == "Symbol":
        symname = o[4]
    elif o[3] == "NUMBER":
        num[o[2]] = str(o[4])
    elif o[3] == "NAME":
        nam[o[2]] = str(o[4])

pairs = sorted(((num[e], nam.get(e)) for e in num), key=lambda t: int(t[0]))
print("SYMBOL:", symname, " pins:", len(pairs))
print("%-6s %s" % ("pad", "pin NAME"))
for n, nm in pairs:
    print("%-6s %s" % (n, nm))
