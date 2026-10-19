import json, os, glob, re

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")
pcb = glob.glob(ROOT + r"\PCB\*.epcb")[0]

recs = []
for ln in open(pcb, encoding="utf-8", errors="replace"):
    ln = ln.strip().rstrip(",")
    if ln.startswith("["):
        try:
            recs.append(json.loads(ln))
        except Exception:
            pass
print("top-level records:", len(recs))

cur = None
desig = {}
pad_owner = {}          # pad element id -> component element id
pad_num = {}            # pad element id -> pad number
nets = {}               # net element id -> net name
for r in recs:
    if r[0] == "COMPONENT":
        cur = r[1]
    elif r[0] == "DESIGNATOR":
        desig[cur] = r[2] if len(r) > 2 else None
    elif r[0] == "PAD":
        pad_owner[r[1]] = cur
        pad_num[r[1]] = r[5]
    elif r[0] == "NET":
        nets[r[1]] = r[2]
    elif r[0] == "PAD_NET":
        pass

net2pads = {}
for r in recs:
    if r[0] == "PAD_NET":
        pid, pnum, pname, nid = r[1], r[2], r[3], r[4]
        comp = pad_owner.get(pid)
        net2pads.setdefault(nid, []).append((desig.get(comp, comp), pnum, pname))

# symbol pin NUMBER -> NAME for U1's symbol (datasheet-faithful)
sym = glob.glob(ROOT + r"\SYMBOL\*.esym")
def sym_map(path):
    rs = []
    for ln in open(path, encoding="utf-8", errors="replace"):
        ln = ln.strip().rstrip(",")
        if ln.startswith("["):
            try: rs.append(json.loads(ln))
            except Exception: pass
    num, nam = {}, {}
    for r in rs:
        if r[0] == "ATTR" and len(r) > 4 and r[3] == "NUMBER": num[r[2]] = r[4]
        if r[0] == "ATTR" and len(r) > 4 and r[3] == "NAME":   nam[r[2]] = r[4]
    return {e: (num.get(e), nam.get(e)) for e in set(num) | set(nam)}

u1sym = None
for s in sym:
    txt = open(s, encoding="utf-8", errors="replace").read()
    if "WROOM" in txt or "IO4" in txt:
        u1sym = s; break
print("U1 symbol:", os.path.basename(u1sym) if u1sym else None)
nm = sym_map(u1sym)

print("\n=== screen nets: raw netlist ===")
for nid, pads in net2pads.items():
    name = nets.get(nid, "?")
    if re.search(r'LCD|TP_|TOUCH|SPI|BL_', str(name), re.I):
        print(f"{name:8s} -> " + ", ".join(f"{d}.pad{p}" for d, p, _ in pads))

print("\n=== U1 module: pad -> net (raw), with datasheet-symbol GPIO name ===")
for d, p, _ in sorted([x for pads in net2pads.values() for x in pads if x[0] == "U1"],
                      key=lambda t: int(t[1])):
    gp = nm.get(p, (None, None))[1]
    netname = [nets.get(nid) for nid, pads in net2pads.items()
               if any(pp[0] == "U1" and pp[1] == p for pp in pads)]
    print(f"U1 pad {p:>2s}  GPIO={str(gp):6s}  net={netname}")
