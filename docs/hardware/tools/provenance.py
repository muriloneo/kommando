import json, os, re, zipfile, glob, collections

REPO = r"C:/codes/esp32/kommando"

print("=== A) firmware/easyeda.json — nets + the pins they touch ===")
p = REPO + "/firmware/easyeda.json"
txt = open(p, encoding="utf-8", errors="replace").read()
try:
    j = json.loads(txt)
    print("top-level keys:", list(j)[:12] if isinstance(j, dict) else type(j))
except Exception as e:
    print("not plain JSON:", e)

# find any net objects with names of interest
for m in re.finditer(r'"name"\s*:\s*"([^"]+)"\s*,\s*"[^"]*"\s*:\s*(\[[^\]]*\])', txt):
    pass
hits = [m.start() for m in re.finditer(r'SPI_|TP_|LCD_|GPIO', txt)]
print("keyword hits:", len(hits))
for h in hits[:40]:
    seg = txt[max(0, h - 120): h + 200].replace("\n", " ")
    print("  ...", re.sub(r"\s+", " ", seg)[:290])

print("\n=== B) U4 connector part + contact count (3rd rev) ===")
z = zipfile.ZipFile(REPO + "/ki-kad/pcb/3rd - Updated_PCB_FIle/ProPrj_Murilofrier_PCB_2026-03-29.epro")
names = z.namelist()
print("members:", [n for n in names][:20])
pcb = [n for n in names if n.endswith(".epcb")][0]
recs = []
for ln in z.read(pcb).decode("utf-8", "replace").splitlines():
    ln = ln.strip()
    if ln.startswith("["):
        try: recs.append(json.loads(ln))
        except Exception: pass
cur = None
comps = collections.defaultdict(dict)
for r in recs:
    if not isinstance(r, list) or not r: continue
    if r[0] == "COMPONENT": cur = r[1]
    if r[0] == "ATTR" and len(r) > 8 and isinstance(r[7], str):
        comps[cur][r[7]] = r[8]
    if r[0] == "PAD": comps[cur].setdefault("_pads", []).append(r[5])
for cid, a in comps.items():
    if a.get("Designator") in ("U4", "U1", "U5"):
        padlist = a.get("_pads", [])
        print(f"  {a.get('Designator')}: pads={len(padlist)} Device={a.get('Device')} Name={a.get('Name')} Footprint={a.get('Footprint')}")
