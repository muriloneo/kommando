import json, os, glob, re

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")
print("files:", sorted(os.listdir(ROOT))[:40])

# --- PCB: raw PAD_NET records for screen nets -------------------------------
pcb = glob.glob(ROOT + r"\PCB\*.epcb")
if not pcb:
    pcb = [p for p in glob.glob(ROOT + r"\**\*", recursive=True) if p.lower().endswith(".epcb")]
print("pcb:", [os.path.basename(p) for p in pcb])
raw = open(pcb[0], encoding="utf-8").read()

print("\n=== RAW PAD_NET records mentioning LCD / TP / screen-ish ===")
for m in re.finditer(r'\["PAD_NET",\s*(.*?)\](?=,\s*\["|\]\s*$)', raw, re.S):
    rec = m.group(0)
    if re.search(r'LCD|TP_|TOUCH|BL_|SPI', rec, re.I):
        print(rec[:300].replace("\n", " "))

print("\n=== RAW PAD_NET for U1 (module) — every record containing U1 ===")
for m in re.finditer(r'\["PAD_NET",\s*"(?:[^"\\]|\\.)*",\s*\[(?:[^\[\]]|\[[^\[\]]*\])*\]\s*\]', raw, re.S):
    rec = m.group(0)
    if '"U1"' in rec:
        print(rec.replace("\n", " ")[:220])
