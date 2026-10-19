import json, glob, os, collections

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")
EPCB = glob.glob(ROOT + r"\PCB\*.epcb")[0]

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

pcb = load(EPCB)

# component id -> attrs
comp_attr = collections.defaultdict(dict)
for o in pcb:
    if o[0] == "ATTR":
        # ["ATTR", attrId, compId, key, value, ...]
        cid, key, val = o[2], o[3], o[4]
        if isinstance(key, str):
            comp_attr[cid][key] = val

comps = [o for o in pcb if o[0] == "COMPONENT"]
print("components:", len(comps))
designators = {}
for c in comps:
    cid = c[1]
    a = comp_attr.get(cid, {})
    des = a.get("Designator")
    designators[cid] = (des, a.get("Footprint"), a.get("Device") or a.get("Name") or a.get("Supplier Part"))
d2c = {}
for cid, (des, fp, dev) in designators.items():
    d2c.setdefault(str(des), []).append(cid)
print("designators:", sorted(k for k in d2c if k != "None"))
print()
for des in sorted(k for k in d2c if k and k != "None"):
    cid = d2c[des][0]
    a = comp_attr[cid]
    print("%-5s %s" % (des, {k: v for k, v in a.items() if k in
          ("Device", "Footprint", "Name", "Manufacturer Part", "Supplier Part", "Value", "LCSC Part")})[:200])

print()
print("--- PAD_NET by component (first 6 comps with pads) ---")
padnets = collections.defaultdict(dict)
for o in pcb:
    if o[0] == "PAD_NET":
        cid, padnum, net = o[1], o[2], o[3]
        padnets[cid][str(padnum)] = net
n = 0
for cid, pads in padnets.items():
    des = designators.get(cid, ("?",))[0]
    print("%-5s (%s): %d pads | %s" % (des, cid, len(pads),
          ", ".join("%s=%s" % (p, v) for p, v in sorted(pads.items(), key=lambda x: (len(x[0]), x[0]))[:14])))
    n += 1
    if n > 8:
        break

# efoo structure
print()
EFOO = glob.glob(ROOT + r"\FOOTPRINT\*\*.efoo")[0]
objs = load(EFOO)
c = collections.Counter(o[0] for o in objs if isinstance(o, list) and o)
print("--- efoo", os.path.basename(EFOO), "record types:", dict(c))
for o in objs:
    if o[0] in ("PAD", "PAD_NET", "COMPONENT", "ATTR"):
        print(json.dumps(o)[:300])
