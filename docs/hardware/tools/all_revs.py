import zipfile, json, os, glob, collections, hashlib

REPO = r"C:/codes/esp32/kommando"
EPROS = sorted(glob.glob(REPO + "/ki-kad/pcb/**/*.epro", recursive=True))
print("found .epro files:")
for e in EPROS:
    print("  ", e, os.path.getsize(e))

# datasheet-confirmed module pad -> GPIO (ESP32-C6-WROOM-1, 3 independent sources)
DS = {'1':'GND','2':'3V3','3':'EN','4':'IO4','5':'IO5','6':'IO6','7':'IO7','8':'IO0','9':'IO1',
      '10':'IO8','11':'IO10','12':'IO11','13':'IO12','14':'IO13','15':'IO9','16':'IO18',
      '17':'IO19','18':'IO20','19':'IO21','20':'IO22','21':'IO23','22':'NC','23':'IO15',
      '24':'IO17/RXD0','25':'IO16/TXD0','26':'IO3','27':'IO2','28':'GND'}

def readlines(src):
    out = []
    for raw in src:
        for ln in raw.decode("utf-8", "replace").splitlines():
            ln = ln.strip()
            if ln.startswith("["):
                try: out.append(json.loads(ln))
                except Exception: pass
    return out

def analyze(epro):
    z = zipfile.ZipFile(epro)
    pcb = [n for n in z.namelist() if n.lower().endswith(".epcb")]
    if not pcb:
        return None
    recs = readlines([z.read(pcb[0])])
    desig = {}
    pads = collections.defaultdict(lambda: collections.defaultdict(list))   # comp -> pad -> nets
    for r in recs:
        if not isinstance(r, list) or not r: continue
        if r[0] == "ATTR" and len(r) > 8 and r[7] == "Designator": desig[r[3]] = r[8]
        if r[0] == "PAD_NET": pads[r[1]][r[2]].append(r[3])
    bydes = {d: pads[c] for c, d in desig.items() if c in pads}
    h = hashlib.sha256(z.read(pcb[0])).hexdigest()[:12]
    return {"pcb": os.path.basename(pcb[0]), "sha": h, "comps": bydes, "all": desig}

results = {}
for e in EPROS:
    try:
        results[e] = analyze(e)
    except Exception as ex:
        print("FAILED", e, ex)

print("\n" + "="*100)
for e, res in results.items():
    if not res: continue
    print(f"\n### {os.path.relpath(e, REPO)}   pcb={res['pcb']} sha={res['sha']}")
    u1, u4 = res["comps"].get("U1", {}), res["comps"].get("U4", {})
    if not u1:
        print("   (no U1 pads parsed) comps:", sorted(res["comps"]))
        continue
    interesting = {}
    for pad, nets in u1.items():
        n = [x for x in nets if x]
        if n: interesting[pad] = n[0]
    print("   U1 module pads carrying a net:")
    for pad in sorted(interesting, key=lambda s: int(s) if s.isdigit() else 999):
        nm = interesting[pad]
        tag = f"GPIO={DS.get(pad,'?'):8s}" if pad in DS else ""
        print(f"      pad {pad:>3s}  {tag} {nm}")
    print("   U4 screen connector:", {p: [x for x in ns if x] for p, ns in
                                      sorted(u4.items(), key=lambda kv: int(kv[0]))})
