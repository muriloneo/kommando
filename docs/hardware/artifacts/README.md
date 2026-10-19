# artifacts/ — index

Raw + decoded products of the hardware investigation. Read
[`../README.md`](../README.md) for the conclusions; these files are the evidence.
**Never regenerate an image or re-extract the netlist to re-answer something here.**

| File | What it is |
|---|---|
| `connector-pinout.md` / `.json` | Decoded pin tables of the module's own schematic — J1 (28-pin panel FPC), J2 (19-contact module FPC that mates the board), J3 (13-pin header). Produced by `../tools/decoded_connectors.py` from the machine-readable `PIJ<conn><nn>` pin records, so it is exact, not eyeballed. |
| `MANIFEST.json` | sha256 + byte size + provenance for every cached script, image, netlist file, doc, and for the locked source files (the three `.epro` board revisions, the module PDF, `README.md`, `config.h`). Use it to detect drift. |
| `sch_words.out.txt` | Full schematic text layer with coordinates (all 638 words) — the raw material every connector decode is derived from. |
| `sch_conn.out.txt` | Connector neighbourhood dumps (J1/J2/J3) with pin records. |
| `sch_full.out.txt` | Connector pin→net reconstruction from text coordinates. |
| `sch_j1.out.txt` | J1 pin list (early attempt, superseded by `connector-pinout.*`). |

Images, and the extracted board netlist, live outside the repo (see `../README.md` §5).

Key facts these files establish:

* the module's FPC (J2) = `1 VCC, 2 LCD_BL, 3 GND, 4 SCLK, 5 MOSI, 6 nc, 7 LCD_DC, 8 LCD_RST,
  9 LCD_CS, 10–12 nc, 13 TP_SCL, 14 TP_SDA, 15 TP_INT, 16–18 nc`, plus contact `0` = GND/shield;
* the module's header (J3) = `1 VCC, 2 GND, 3 MOSI, 4 SCLK, 5 LCD_CS, 6 LCD_DC, 7 LCD_RST,
  8 LCD_BL, 9 TP_SDA, 10 TP_SCL, 11 TP_RST, 12 TP_INT` — **the order the board's net labels
  were taken from**, which is why U4 pads 4/5 and 13/14 are labelled crossed.
