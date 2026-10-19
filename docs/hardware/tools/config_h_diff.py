#!/usr/bin/env python
"""Build the PROPOSED config.h pin fix as a diff -- repo file is NOT touched.

Board authority (EasyEDA Pro project, scratch copy):
  U1 = ESP32-C6-WROOM-1-N8. Symbol pin NUMBER -> NAME, PCB PAD_NET for U1:
    pad 6 -> LCD_DIN (IO6)        pad 7  -> LCD_CLK (IO7)
    pad 26 -> LCD_CS (IO3)        pad 11 -> LCD_DC (IO10)
    pad 12 -> LCD_RST (IO11)      pad 27 -> LCD_BL (IO2)
    pad 5 -> TP_SDA (IO5)         pad 4  -> TP_SCL (IO4)
    pad 18 -> TP_INT (IO20)       pad 17 -> TP_RST (IO19)
    pad 23 -> $1N8314 via R6 -> LED2 (IO15)   pad 8 -> BOOT SW2+R12 (IO0)
    pad 15 -> unconnected (IO9 = strap)
"""
import hashlib
import os
import re
import subprocess

SRC = r"C:\codes\esp32\kommando\firmware\main\config.h"
TMP = os.path.expandvars(r"%LOCALAPPDATA%\Temp\kommando_config_patch")
os.makedirs(TMP, exist_ok=True)
COPY = os.path.join(TMP, "config.h")

raw = open(SRC, "rb").read()
before = hashlib.sha256(raw).hexdigest()
crlf = b"\r\n" in raw
s = raw.replace(b"\r\n", b"\n").decode("utf-8")

fails = []


def apply(pattern, repl, tag, want=1):
    global s
    s, n = re.subn(pattern, repl, s, flags=re.M)
    if n != want:
        fails.append(f"{tag}: matched {n}, expected {want}")


# --- doc-table rows: keep the GPIO column aligned (field width 7) -----------
ROWS = [
    (r"(^\s+3\s+Orange\s+)", 6, "SPI MOSI", "doc LCD FPC3"),
    (r"(^\s+4\s+Green\s+)", 7, "SPI CLK", "doc LCD FPC4"),
    (r"(^\s+5\s+Blue\s+)", 3, "SPI CS", "doc LCD FPC5"),
    (r"(^\s+6\s+Purple\s+)", 10, "Data / Command (DC)", "doc LCD FPC6"),
    (r"(^\s+7\s+Gray\s+)", 11, "LCD Reset", "doc LCD FPC7"),
    (r"(^\s+8\s+Orange\s+)", 2, "Backlight (LEDC PWM)", "doc LCD FPC8"),
    (r"(^\s+9\s+Green\s+)", 5, "I2C SDA", "doc touch FPC9"),
    (r"(^\s+10\s+Blue\s+)", 4, "I2C SCL", "doc touch FPC10"),
    (r"(^\s+12\s+Gray\s+)", 20,
     "INT (active low; IO20 cannot wake - LP_IO wake is GPIO0-7)", "doc touch FPC12"),
    (r"(^\s+16\s+\S+\s+)", 19, "Touch reset", "doc touch FPC16"),
]
for prefix, num, label, tag in ROWS:
    apply(prefix + r"\d+\s+.*$",
          (lambda n, l: (lambda m: m.group(1) + str(n).ljust(7) + l))(num, label),
          tag)

# --- pin defines: right pad the GPIO field to column 13 --------------------
DEFS = [
    ("PIN_LCD_MOSI", 6), ("PIN_LCD_CLK", 7), ("PIN_LCD_CS", 3), ("PIN_LCD_DC", 10),
    ("PIN_LCD_RST", 11), ("PIN_LCD_BL", 2), ("PIN_TOUCH_SDA", 5), ("PIN_TOUCH_SCL", 4),
    ("PIN_TOUCH_INT", 20), ("PIN_TOUCH_RST", 19), ("PIN_STATUS_LED", 15), ("PIN_BUTTON", 0),
]
for name, num in DEFS:
    apply(r"(^#define\s+" + name + r"\s+)GPIO_NUM_\d+(\s+)(/\*.*)$",
          (lambda n: (lambda m: m.group(1) + ("GPIO_NUM_%d" % n).ljust(13) + m.group(3)))(num),
          "define " + name)

# --- claims that stop being true once INT sits on IO20 ---------------------
apply(r"/\* Deep-sleep wake capable \(C6: GPIO 0-7\) \*/",
      "/* NOT a wake source: C6 LP_IO wake is GPIO0-7 only */", "deep-sleep note")

if fails:
    print("SUBSTITUTION FAILURES:")
    for f in fails:
        print("  -", f)
    raise SystemExit(1)

out = s.replace("\n", "\r\n") if crlf else s
open(COPY, "wb").write(out.encode("utf-8"))

after = hashlib.sha256(open(SRC, "rb").read()).hexdigest()
print("repo file sha256 before:", before)
print("repo file sha256 after :", after)
print("repo file untouched    :", before == after)
print("line endings           :", "CRLF (preserved)" if crlf else "LF")
print("substitutions applied  :", len(ROWS) + len(DEFS) + 1, "checks, 0 failures")
print("proposed copy          :", COPY)
print()

d = subprocess.run(["diff", "-u", "--label", "a/firmware/main/config.h",
                    "--label", "b/firmware/main/config.h  (proposed)",
                    SRC, COPY], capture_output=True, text=True)
print(d.stdout.replace("\r", ""))
print("diff exit code:", d.returncode, "(1 = differences found)")
