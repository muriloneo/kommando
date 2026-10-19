import json, os, glob, collections

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

def sym_pins(uuid):
    """element id -> (NUMBER, NAME) for every pin of a symbol."""
    path = glob.glob(ROOT + r"\SYMBOL\\" + uuid[:12] + "*.esym")[0]
    obj = load(path)
    num, nam, symname = {}, {}, None
    for o in obj:
        if not (isinstance(o, list) and o):
            continue
        if o[0] == "ATTR" and len(o) > 4:
            if o[2] == "" and o[3] == "Symbol":
                symname = o[4]
            elif o[3] == "NUMBER":
                num[o[2]] = str(o[4])
            elif o[3] == "NAME":
                nam[o[2]] = str(o[4])
    return {e: (num.get(e), nam.get(e)) for e in num}, symname

# --- PCB netlist (component id -> designator, pad -> net) ---
pcb = load(glob.glob(ROOT + r"\PCB\*.epcb")[0])
pattr = collections.defaultdict(dict)
for o in pcb:
    if isinstance(o, list) and o and o[0] == "ATTR" and len(o) > 8 and isinstance(o[7], str):
        pattr[o[3]][o[7]] = o[8]
padnet = collections.defaultdict(dict)
for o in pcb:
    if isinstance(o, list) and o and o[0] == "PAD_NET":
        padnet[o[1]][str(o[2])] = o[3]
des2cid = {}
for cid, a in pattr.items():
    d = a.get("Designator")
    if d:
        des2cid.setdefault(d, cid)

u1cid, u4cid = des2cid["U1"], des2cid["U4"]

# --- schematic -> designator -> symbol uuid (U1 already known from earlier run) ---
U1_SYM = "7676aead2dfc4e839020fd4f7b51e58b"
U4_SYM = "a21dd8ab47394e47bde57569d5448f46"

u1pins, u1name = sym_pins(U1_SYM)
u4pins, u4name = sym_pins(U4_SYM)
print("symbol U1 =", u1name, "| pins:", len(u1pins))
print("symbol U4 =", u4name, "| pins:", len(u4pins))

# U1 pad number -> GPIO name (from symbol NUMBER->NAME via element id)
u1_num2name = {}
for e, (n, nm) in u1pins.items():
    if n:
        u1_num2name[n] = nm

u4_num2name = {}
for e, (n, nm) in u4pins.items():
    if n:
        u4_num2name[n] = nm

u1nets = {p: n for p, n in padnet[u1cid].items() if n}
u4nets = {p: n for p, n in padnet[u4cid].items() if n}
net2u1 = {n: p for p, n in u1nets.items()}

print()
print("=== U4 (screen connector) pin by pin -> ESP32-C6 ===")
print("%-5s %-10s %-10s %-8s %s" % ("U4pin", "net", "C6 pad", "GPIO", "U4 sym pin name"))
for p in sorted(u4nets, key=int):
    net = u4nets[p]
    c6 = net2u1.get(net)
    gpio = u1_num2name.get(c6, "?") if c6 else ("-" if net in ("GND", "3.3V") else "not on C6")
    print("%-5s %-10s %-10s %-8s %s" % (p, net, c6 or "-", gpio, u4_num2name.get(p)))

print()
print("=== full U1 pad table (module pin -> net -> GPIO name) ===")
for p in sorted(u1nets, key=int):
    net = u1nets[p]
    others = []
    for cid, pads in padnet.items():
        d = pattr.get(cid, {}).get("Designator")
        if d and d not in ("U1",) and net in pads.values():
            others.append(d)
    print("%-5s %-9s %-10s also on: %s" % (p, u1_num2name.get(p), net, ",".join(sorted(set(others)))))
