#!/usr/bin/env python3
"""Cache + manifest builder for the Kommando hardware investigation (2026-09-14).

Copies every heavy-processing artifact out of %LOCALAPPDATA%\\Temp (ephemeral)
into two durable homes:
  A) repo   docs/hardware/tools/            -> the analysis scripts (text, versioned)
  B) cache  %LOCALAPPDATA%\\hermes\\cache\\kommando-hardware/
            images/            -> rendered schematic pages + crops (do NOT re-render)
            netlist-3rd-rev/   -> extracted EasyEDA netlist of the current board
  and writes docs/hardware/artifacts/MANIFEST.json (sha256 + provenance).
Safe to re-run: idempotent copies, manifest regenerated.
"""
import hashlib, json, os, shutil, sys

TEMP = os.path.expandvars(r"%LOCALAPPDATA%\Temp")
CACHE = os.path.expandvars(r"%LOCALAPPDATA%\hermes\cache\kommando-hardware")
REPO = r"C:/codes/esp32/kommando"
DOCS = os.path.join(REPO, "docs", "hardware")
TOOLS = os.path.join(DOCS, "tools")
ART = os.path.join(DOCS, "artifacts")

SCRIPTS = {
    "all_revs.py": "enumerate ALL .epro revisions in ki-kad/pcb/ + netlist diff",
    "board_probe.py": "first raw .epro container probe",
    "board_probe2.py": "netlist collection discovery in .epro",
    "board_probe3.py": "U3 pin1->pin3 bodge check",
    "board_probe4.py": "PAD_NET record structure",
    "board_probe5.py": "component designator -> package map",
    "board_probe6.py": "net -> pin membership dump",
    "board_final.py": "FINAL board netlist: every component pin -> net",
    "raw_netdump.py": "raw PAD_NET records for screen nets + module pad->net table",
    "netmap_verify.py": "PCB *.epcb PAD_NET joined to SYMBOL pin NAME/NUMBER",
    "sym_pins.py": "symbol PIN/ATTR structure (pin number/name pairing method)",
    "u1_full_pinmap.py": "U1 (ESP32-C6-WROOM-1) pad -> net, all 37 pads",
    "screen_pins_final.py": "screen pin extraction attempt (exit 1 - superseded by netmap_verify.py)",
    "render_sch.py": "render module schematic PDF page -> PNG (pymupdf)",
    "sch_words.py": "module schematic text layer with coordinates",
    "sch_conn.py": "connector neighbourhood dump (J1/J2/J3)",
    "sch_full.py": "reconstruct connector pin->net tables from text coords",
    "sch_j1.py": "J1 full pin list",
    "sch_crops.py": "tight high-DPI crops (sch_j1/j2/j3.png) for vision",
    "provenance.py": "trace firmware/easyeda.json netlist vs current board (README table origin)",
    "config_h_diff.py": "config.h HEAD vs working-tree pin diff",
    "config_h_patch_v3.py": "config.h pin patch generator (v3)",
    "config_h_patch_v4.py": "config.h pin patch generator (v4)",
    "ci_patch.py": "CI rework: one release tag per branch (became commit e554221)",
}
IMAGES = ["lcd_sch_p0.png", "sch_j1.png", "sch_j2.png", "sch_j3.png"]
LOCKED = ["README.md", r"firmware\main\config.h",
          r"ki-kad\1.9inch_Touch_LCD_Schematic.pdf",
          r"ki-kad\pcb\first-version-Esp32_Based_PCB_Design\Project Source File Easyeda\ProPrj_Murilofrier_PCB_2026-03-23.epro",
          r"ki-kad\pcb\2nd-version\ESP32_DISP_PCB\Source File\ProPrj_Murilofrier_PCB_2026-03-23.epro",
          r"ki-kad\pcb\3rd - Updated_PCB_FIle\ProPrj_Murilofrier_PCB_2026-03-29.epro"]


def sha256(p, buf=1 << 20):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while chunk := f.read(buf):
            h.update(chunk)
    return h.hexdigest()


def main():
    for d in (TOOLS, ART, os.path.join(CACHE, "images"), os.path.join(CACHE, "netlist-3rd-rev")):
        os.makedirs(d, exist_ok=True)
    man = {"generated": "see git/commit e554221 era, session 2026-09-14",
           "canonical_board_rev": "ki-kad/pcb/3rd - Updated_PCB_FIle/ProPrj_Murilofrier_PCB_2026-03-29.epro",
           "rule": "Never re-render or re-read these images; never re-extract the netlist. Use the decoded tables in ../README.md and these files.",
           "scripts": {}, "images": {}, "netlist": {}, "locked_hashes": {}}

    miss = []
    for name, why in SCRIPTS.items():
        src = os.path.join(TEMP, name)
        if not os.path.exists(src):
            miss.append(name)
            continue
        dst = os.path.join(TOOLS, name)
        shutil.copy2(src, dst)
        man["scripts"][name] = {"sha256": sha256(dst), "bytes": os.path.getsize(dst), "answers": why,
                                "tool": f"docs/hardware/tools/{name}"}
    for name in IMAGES:
        src = os.path.join(TEMP, name)
        if not os.path.exists(src):
            miss.append(name)
            continue
        dst = os.path.join(CACHE, "images", name)
        shutil.copy2(src, dst)
        man["images"][name] = {"sha256": sha256(dst), "bytes": os.path.getsize(dst),
                               "path": dst, "note": "cached render - do NOT regenerate"}
    sroot = os.path.join(TEMP, "kommando_u3", "epro")
    if os.path.isdir(sroot):
        for dp, _dn, fn in os.walk(sroot):
            for f in fn:
                sp = os.path.join(dp, f)
                dpi = os.path.join(CACHE, "netlist-3rd-rev", os.path.relpath(sp, sroot))
                os.makedirs(os.path.dirname(dpi), exist_ok=True)
                shutil.copy2(sp, dpi)
        for dp, _dn, fn in os.walk(os.path.join(CACHE, "netlist-3rd-rev")):
            for f in fn:
                dp_ = os.path.join(dp, f)
                man["netlist"][os.path.relpath(dp_, os.path.join(CACHE, "netlist-3rd-rev")).replace("\\", "/")] = \
                    {"sha256": sha256(dp_), "bytes": os.path.getsize(dp_)}
    for rel in LOCKED:
        p = os.path.join(REPO, rel)
        if os.path.exists(p):
            man["locked_hashes"][rel.replace("\\", "/")] = {"sha256": sha256(p), "bytes": os.path.getsize(p)}
    man["docs"] = {}
    for dp, _dn, fn in os.walk(DOCS):
        for f in fn:
            p = os.path.join(dp, f)
            man["docs"][os.path.relpath(p, DOCS).replace("\\", "/")] = \
                {"sha256": sha256(p), "bytes": os.path.getsize(p)}
    with open(os.path.join(ART, "MANIFEST.json"), "w", encoding="utf-8") as f:
        json.dump(man, f, indent=2)
    print(f"scripts cached : {len(man['scripts'])}/{len(SCRIPTS)}")
    print(f"images  cached : {len(man['images'])}/{len(IMAGES)}")
    print(f"netlist files  : {len(man['netlist'])}")
    print(f"hashed sources : {len(man['locked_hashes'])}")
    if miss:
        print("MISSING:", miss)
    print("manifest:", os.path.join(ART, "MANIFEST.json"))


if __name__ == "__main__":
    sys.exit(main())
