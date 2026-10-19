# Kommando hardware — display & touch wiring (verified reference)

Status: **verified 2026-09-14** against the repository's own hardware files (board netlist +
the module's own schematic). Everything needed to answer "which GPIO does the screen use?"
is in this folder — **do not re-render the schematic PDF, re-read it as an image, or re-extract
the board netlist.** See [Cache](#5-cache--do-not-redo-this-work).

Firmware side: pins applied to `firmware/main/config.h` on branch `esp32c6-untested`
(**uncommitted / unpushed / not yet bench-tested**).

---

## 1. The mapping

Module FPC contact numbers and board U4 pad numbers are **identical** (the board was laid out
against the module's 20-contact ribbon: pad 1 = 3V3 at the same end, same pads left unconnected).

| Signal | module FPC (J2) | board U4 pad | ESP32-C6 | `config.h` define |
|---|---|---|---|---|
| 3V3 | 1 | 1 | 3V3 | — |
| LCD backlight | 2 | 2 | **GPIO2** | `PIN_LCD_BL` |
| GND | 3 (+ `0`/shield) | 3 (+ 18) | GND | — |
| LCD SCLK (clock) | **4** | 4 | **GPIO6** | `PIN_LCD_CLK` |
| LCD MOSI (data) | **5** | 5 | **GPIO7** | `PIN_LCD_MOSI` |
| — | 6 | 6 | — | *nc* |
| LCD DC | 7 | 7 | **GPIO10** | `PIN_LCD_DC` |
| LCD RST | 8 | 8 | **GPIO11** | `PIN_LCD_RST` |
| LCD CS | 9 | 9 | **GPIO3** | `PIN_LCD_CS` |
| — | 10–12 | 10–12 | — | *nc* |
| Touch SCL | **13** | 13 | **GPIO5** | `PIN_TOUCH_SCL` |
| Touch SDA | **14** | 14 | **GPIO4** | `PIN_TOUCH_SDA` |
| Touch INT | 15 | 15 | **GPIO20** | `PIN_TOUCH_INT` |
| Touch RST | *16, unlabelled on the module* | 16 | **GPIO19** | `PIN_TOUCH_RST` |
| GND | — | 18 | GND | — |

**The screen occupies 10 GPIOs: 2, 3, 4, 5, 6, 7, 10, 11, 19, 20** (+3V3, GND).
None of the C6 strapping pins (IO8, IO9, IO15) is used by the screen.
Elsewhere on the board: button = **IO0** (net `BOOT`), WS2812 status LED = **IO15**, USB = IO12/IO13.

Touch INT (IO20) is outside the C6's LP_IO range (GPIO0–7), so **touch cannot wake from deep
sleep**; only the button (IO0) can.

---

## 2. How this was established

| Result | Source | Method / tool |
|---|---|---|
| pad → GPIO for every U4 pad | `ki-kad/pcb/3rd - Updated_PCB_FIle/...2026-03-29.epro` (the current board) | `PAD_NET` records joined to the U1 symbol's `PIN`+`ATTR` records → `tools/netmap_verify.py`, `tools/board_final.py`, `tools/all_revs.py` |
| which pad carries which signal | `ki-kad/1.9inch_Touch_LCD_Schematic.pdf` (the module's own schematic) | machine-readable pin records `PIJ<conn><nn>` paired with the text drawn on the same row → `tools/decoded_connectors.py` (no image reading involved) |
| C6 pad → GPIO names | ESP32-C6-WROOM-1 datasheet + Espressif DevKitC docs | cross-checked, board symbol confirmed faithful (pad 6=IO6, 7=IO7, 11=IO10, 12=IO11, 17=IO19, 18=IO20, 26=IO3, 27=IO2) |

Decoded connector tables (all three connectors of the module) are in
[`artifacts/connector-pinout.md`](artifacts/connector-pinout.md) / `.json`.

---

## 3. The two crossed pairs — the actual trap

The module's schematic exposes the same signals twice:

* **J2** — the 19-contact FPC that mates the board: `4=SCLK, 5=MOSI … 13=TP_SCL, 14=TP_SDA`
* **J3** — a 13-pin header (0…12): `3=MOSI, 4=SCLK … 9=TP_SDA, 10=TP_SCL, 11=TP_RST, 12=TP_INT`

The board's net **labels** on U4 pads 4/5 and 13/14 follow **J3's order**, i.e. they are crossed
relative to the physical ribbon: the net called `LCD_DIN` sits on the SCLK contact and `LCD_CLK`
on the MOSI contact; `TP_SDA` sits on the TP_SCL contact and `TP_SCL` on the TP_SDA contact.
The **copper** decides, not the label — and the copper is right: IO6 is the C6's `FSPICLK` and
IO7 its `FSPID`, so clock lands on the clock pin and data on the data pin.

Everywhere else the labels agree with the module (pads 1, 2, 3, 7, 8, 9, 15), which is why
only these two pairs ever looked plausible.

---

## 4. Where the wrong table came from

`firmware/easyeda.json` is the **early dual-connector design**, not this board:

```
SPI_CLK  : [U1,GPIO6] <-> [J2,LCD_CLK]      SPI_MOSI : [U1,GPIO7] <-> [J2,LCD_DIN]
LCD_CS   : GPIO10      LCD_DC : GPIO11      LCD_RST  : GPIO3       LCD_BL : GPIO2
I2C_SDA  : GPIO0 <-> U3.TP_SDA   I2C_SCL : GPIO1 <-> U3.TP_SCL
TOUCH_RST: GPIO20                TOUCH_INT: GPIO19                 button : GPIO8
```

That table (also copied into `README.md` ~lines 155–169, `firmware/repo-context.md:113`,
`firmware/.ai/skills/debug.md:10`, and the pre-edit `config.h`) is **stale**: this board uses
CS=IO3, DC=IO10, RST=IO11 and put the touch on IO4/IO5, because the single 20-contact FPC
forced a different pin order.
Two rows of it cannot work on the current board at all: **IO0 is the BOOT button net**
(SW2 + R12 pull-up) and **IO1 is not routed to the connector**.

---

## 5. Cache — do NOT redo this work

| What | Where |
|---|---|
| rendered schematic pages + crops (PNG) | `%LOCALAPPDATA%\hermes\cache\kommando-hardware\images\` |
| extracted EasyEDA netlist of the current board (93 files) | `%LOCALAPPDATA%\hermes\cache\kommando-hardware\netlist-3rd-rev\` |
| analysis scripts (26) | [`tools/`](tools/) — index of what each one answers is in `tools/`'s header comments + `artifacts/MANIFEST.json` |
| decoded connector tables + raw text dumps + sha256 manifest | [`artifacts/`](artifacts/) |

Refresh the cache (idempotent): `python docs/hardware/tools/cache_artifacts.py`.
`%LOCALAPPDATA%\Temp` is **ephemeral** — nothing is kept there any more.

If a result ever has to be re-derived from scratch, the two commands that matter are:

```
python docs/hardware/tools/netmap_verify.py      # board: U4 pad -> net -> C6 pad -> GPIO
uv run --with pymupdf python docs/hardware/tools/decoded_connectors.py   # module: connector pin tables
```

---

## 6. Open items

1. **Bench proof (30 s):** buzz continuity from a module ribbon contact to the C6 pad —
   contact **4 must reach IO6** (not IO7). If the panel instead comes up blank and touch is
   dead, that is the signature of the two pairs being inverted; it is one constant each.
2. **`PIN_TOUCH_RST` (IO19):** the board ties pad 16 to IO19, but the module's FPC pin 16
   carries no label in the vendor schematic (`TP_RST` is declared only on its J3 header,
   pin 11). The line may be dead on the ribbon — drive it anyway (harmless), but do not rely
   on a software touch-reset until this is checked.
3. **Stale docs to fix:** `README.md` (wiring table + "Button | GPIO9"), `firmware/repo-context.md`,
   `firmware/.ai/skills/debug.md` still carry the old design's numbers.

---

## 7. Revision inventory

| Revision | File | Note |
|---|---|---|
| 1st | `ki-kad/pcb/first-version-Esp32_Based_PCB_Design/.../ProPrj_..._2026-03-23.epro` | 12-pin connector, I2C on IO8/IO9 |
| 2nd | `ki-kad/pcb/2nd-version/ESP32_DISP_PCB/Source File/ProPrj_..._2026-03-23.epro` | 12-pin connector (U5) |
| **3rd (current)** | `ki-kad/pcb/3rd - Updated_PCB_FIle/ProPrj_..._2026-03-29.epro` | **20-contact FPC (U4) that mates the module — this is the one this document describes** |

All three differ; sha256 of each is in [`artifacts/MANIFEST.json`](artifacts/MANIFEST.json).
