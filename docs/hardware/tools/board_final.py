import json, os, collections, glob

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

# ---------- schematic -> designator, symbol uuid, pin names ----------
sch = load(glob.glob(ROOT + r"\SHEET\*\*.esch")[0])
attrs = collections.defaultdict(dict)
for o in sch:
    if isinstance(o, list) and o and o[0] == "ATTR" and len(o) > 4:
        attrs[o[2]][o[3]] = o[4]

comps = [o for o in sch if isinstance(o, list) and o and o[0] == "COMPONENT"]
print("components in schematic:", len(comps))

# pin names per symbol uuid
pinname_cache = {}
def pin_names(sym_uuid):
    if sym_uuid in pinname_cache:
        return pinname_cache[sym_uuid]
    path = ROOT + r"\SYMBOL\\" + sym_uuid + ".esym"
    d = {}
    if os.path.exists(path):
        for o in load(path):
            if isinstance(o, list) and o and o[0] == "PIN":
                # ["PIN","e5","1","IO4", ...]
                num, name = o[2], o[3]
                d[str(num)] = str(name)
    pinname_cache[sym_uuid] = d
    return d

designator = {}
for c in comps:
    cid = c[1]
    a = attrs.get(cid, {})
    designator[cid] = (a.get("Designator"), a.get("Symbol"), a.get("Device"), a.get("Name"))

for cid, t in sorted(designator.items(), key=lambda x: str(x[1][0])):
    print("%-6s sym=%s dev=%s name=%s" % (str(t[0]), str(t[1])[:12], str(t[2])[:12], str(t[3])[:28]))

# ---------- PCB netlist ----------
pcb = load(glob.glob(ROOT + r"\PCB\*.epcb")[0])
pattrs = collections.defaultdict(dict)
for o in pcb:
    if isinstance(o, list) and o and o[0] == "ATTR" and len(o) > 8 and isinstance(o[7], str):
        pattrs[o[3]][o[7]] = o[8]
padnet = collections.defaultdict(dict)
for o in pcb:
    if isinstance(o, list) and o and o[0] == "PAD_NET":
        padnet[o[1]][str(o[2])] = o[3]

# pcb comp id -> designator (via schematic designator matched on Device/Footprint is messy:
# the PCB and SCH component ids differ; match by the "Symbol"/"Device" uuid present in both)
p_des = {}
for cid, a in pattrs.items():
    p_des[cid] = a.get("Designator")
# build net -> {designator: [pads]}
netmap = collections.defaultdict(lambda: collections.defaultdict(list))
for cid, pads in padnet.items():
    des = p_des.get(cid) or "?"
    for pad, net in pads.items():
        if net:
            netmap[net][des].append(pad)

print()
print("=== nets touching U1 or U4 ===")
for net in sorted(netmap):
    d = netmap[net]
    if "U1" in d or "U4" in d:
        parts = "; ".join("%s:%s" % (k, ",".join(sorted(v, key=lambda s: (0, int(s)) if s.isdigit() else (1, s)))) for k, v in sorted(d.items()))
        print("%-12s %s" % (net, parts))
