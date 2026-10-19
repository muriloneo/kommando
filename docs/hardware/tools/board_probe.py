import json, glob, os, itertools

ROOT = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_u3\epro")

def load_lines(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for ln in f:
            ln = ln.strip()
            if not ln:
                continue
            try:
                out.append(json.loads(ln))
            except Exception as e:
                pass
    return out

def show(path, maxlen=2000):
    print("=" * 100)
    print(path, os.path.getsize(path), "bytes")
    objs = load_lines(path)
    print("json-lines:", len(objs))
    for o in objs[:6]:
        if isinstance(o, dict):
            print("  doctype:", o.get("docType"), "| keys:", list(o.keys())[:25])
            for k, v in o.items():
                if isinstance(v, list):
                    print("     ", k, "list", len(v), (list(v[0].keys())[:18] if v and isinstance(v[0], dict) else ""))
                elif isinstance(v, dict):
                    print("     ", k, "dict", list(v.keys())[:18])
        else:
            print("  ", repr(o)[:120])

epcb = glob.glob(ROOT + r"\PCB\*.epcb")[0]
show(epcb)
show(glob.glob(ROOT + r"\SHEET\*\*.esch")[0])
# footprint + symbol for U1 if known
show(ROOT + r"\SYMBOL\796daf2aae5649c48b006c63f233621f.esym")
for p in glob.glob(ROOT + r"\FOOTPRINT\*\*.efoo")[:3]:
    show(p)
