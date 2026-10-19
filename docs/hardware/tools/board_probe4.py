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

# PCB ATTR shape: ["ATTR", attrId, 0, <compId>, 3, null, null, <key>, <value>, ...]
comp_attr = collections.defaultdict(dict)
for o in pcb:
    if o[0] == "ATTR" and len(o) > 8 and isinstance(o[7], str):
        comp_attr[o[3]][o[7]] = o[8]

padnets = collections.defaultdict(dict)
for o in pcb:
    if o[0] == "PAD_NET":
        padnets[o[1]][str(o[2])] = o[3]

def pad_sort(p):
    return (0, int(p)) if p.isdigit() else (1, p)

comps = [(c[1], comp_attr.get(c[1], {})) for c in pcb if c[0] == "COMPONENT"]
print("=== designators ===")
for cid, a in sorted(comps, key=lambda x: str(x[1].get("Designator"))):
    print("%-6s %-34s fp=%s" % (str(a.get("Designator")), str(a.get("Device") or a.get("Name") or a.get("Value"))[:34],
                                str(a.get("Footprint"))[:12]))

print()
print("=== U1-like candidates (most pads) ===")
for cid, pads in sorted(padnets.items(), key=lambda x: -len(x[1]))[:4]:
    a = comp_attr.get(cid, {})
    print("%s %s -> %d pads: %s" % (a.get("Designator"), a.get("Device") or a.get("Name"), len(pads),
          ", ".join("%s=%s" % (p, n) for p, n in sorted(pads.items(), key=lambda x: pad_sort(x[0])))))

# save intermediate for next step
import pickle
pickle.dump({"comp_attr": dict(comp_attr), "padnets": dict(padnets),
             "comps": [(c, a) for c, a in comps], "root": ROOT},
            open(os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_board.pkl"), "wb"))
print()
print("saved pickle")
