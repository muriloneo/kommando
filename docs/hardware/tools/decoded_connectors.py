#!/usr/bin/env python3
"""Decode the module's own schematic (ki-kad/1.9inch_Touch_LCD_Schematic.pdf) into
authoritative connector pin tables.

Instead of eyeballing a rendered image, this pairs every machine-readable pin record
(`PIJ<conn><nn>`, e.g. PIJ301 = J3.01) with the text drawn on the same row, and writes:
  docs/hardware/artifacts/connector-pinout.json   (machine readable)
  docs/hardware/artifacts/connector-pinout.md     (human readable)

Run:  uv run --with pymupdf python docs/hardware/tools/decoded_connectors.py
Do NOT re-render the PDF to an image and re-read it - this is the cached decode.
"""
import json, os, re, sys

PDF = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "..", "..", "ki-kad", "1.9inch_Touch_LCD_Schematic.pdf")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "artifacts")
ANCHORS = {"J1": (700, 70, 860, 270), "J2": (55, 150, 145, 300), "J3": (195, 170, 245, 300)}
# (x0, y0, x1, y1) box per connector: narrow enough to exclude neighbouring columns.


def load_words():
    import fitz
    doc = fitz.open(PDF)
    words = []
    for pno, page in enumerate(doc):
        for x0, y0, x1, y1, text, *_ in page.get_text("words"):
            words.append((pno, round(x0, 1), round(y0, 1), text))
    return words


def main():
    words = load_words()
    result = {}
    for conn in ANCHORS:
        rows = {}
        for pno, x, y, t in words:
            m = re.fullmatch(r"PIJ%s(\d+)" % conn[1], t)
            if m:
                rows.setdefault(round(y, 1), (int(m.group(1)), x))
        table = []
        for py in sorted(rows):
            num, px = rows[py]
            near = []
            for pno, x, y, t in words:
                if t.startswith(("PIJ", "COJ", "NL", "PI")) or re.fullmatch(r"\d+", t):
                    continue
                if abs(y - py) <= 4.0 and -45 <= (x - px) <= 45:
                    near.append({"x": x, "text": t, "dx": round(x - px, 1)})
            near.sort(key=lambda d: (abs(d["dx"]), d["dx"]))
            table.append({"pin": num, "row_y": py,
                          "label": near[0]["text"] if near else None,
                          "candidates": near})
        table.sort(key=lambda r: r["pin"])
        result[conn] = table

    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "connector-pinout.json"), "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)

    lines = ["# Module schematic - decoded connector pin tables",
             "",
             "Source: ki-kad/1.9inch_Touch_LCD_Schematic.pdf (text layer, pin records PIJ<conn><nn>).",
             "Decoded by docs/hardware/tools/decoded_connectors.py - do not re-render/re-read the image.",
             ""]
    for conn, table in result.items():
        lines += [f"## {conn}", "", "| pin | signal (nearest text to the pin) | candidates dx |", "|---|---|---|"]
        for r in table:
            cands = ", ".join(f"{c['text']}@{c['dx']:+.0f}" for c in r["candidates"][:4])
            lines.append(f"| {r['pin']} | {r['label'] or '-'} | {cands} |")
        lines.append("")
    with open(os.path.join(OUT, "connector-pinout.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("\n".join(lines))


if __name__ == "__main__":
    sys.exit(main())
