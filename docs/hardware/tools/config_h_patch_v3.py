#!/usr/bin/env python
"""v3: apply the config.h pin fix preserving column alignment EXACTLY.

Rules:
  * define lines  -> replace the GPIO number, re-pad the whitespace AFTER it
                     (the "GPIO_NUM_" prefix is glued to the digits).
  * doc-table rows-> replace the number, then absorb/emit the width delta in the
                     whitespace BEFORE it, so the label column never moves.
Source of truth for the numbers: EasyEDA board netlist (see v1 script header).
"""
import re
import subprocess

SRC = r"C:\codes\esp32\kommando\firmware\main\config.h"
COPY = r"C:\Users\Murilo\AppData\Local\Temp\kommando_config_patch\config.h"

# reuse the pristine HEAD text so the patch is derived from the committed file
orig = subprocess.run(["git", "-C", r"C:\codes\esp32\kommando", "show",
                       "HEAD:firmware/main/config.h"],
                      capture_output=True).stdout
orig = orig.replace(b"\r\n", b"\n").decode("utf-8")
s = orig
fails = []


def swap_after_ws(prefix, digits, tail, num, tag):
    """keep `tail` frozen: pad/trim the whitespace that sits before it."""
    delta = len(digits) - len(num)          # >0 => number got shorter
    if delta > 0:
        new_tail = " " * delta + tail
    elif delta < 0:
        need = -delta
        if not tail.startswith(" " * need):
            fails.append("%s: tail too tight to absorb %d cols" % (tag, need))
            return None
        new_tail = tail[need:]
    else:
        new_tail = tail
    return prefix + num + new_tail


def rep_define(name, num):
    global s
    pat = r"^#define\s+" + name + r"\s+GPIO_NUM_(\d+)(\s+)(/\*.*)$"
    m = re.search(pat, s, flags=re.M)
    if not m:
        fails.append("define %s: not found" % name)
        return
    line = m.group(0)
    digits = m.group(1)
    gl = line.index("GPIO_NUM_") + len("GPIO_NUM_")
    new = swap_after_ws(line[:gl], digits, line[gl + len(digits):], str(num),
                        "define " + name)
    if new:
        s = s.replace(line, new, 1)


def rep_row(prefix_pat, num, label, tag):
    """doc-table row: absorb the delta in the spaces BEFORE the number."""
    global s
    pat = prefix_pat + r"(\d+)(\s+)(.*)$"
    m = re.search(pat, s, flags=re.M)
    if not m:
        fails.append("%s: not found" % tag)
        return
    line = m.group(0)
    digits = m.group(1)
    before = line[:len(line) - len(line.lstrip())]      # leading indent
    mid = line[:m.start(1)]                              # up to the digits
    num = str(num)
    delta = len(num) - len(digits)
    if delta > 0:                                        # need a column
        if not mid.endswith(" " * delta):
            fails.append("%s: no space to steal" % tag)
            return
        mid = mid[:-delta]
    elif delta < 0:
        mid = mid + " " * (-delta)
    new = mid + num + m.group(2) + label
    s = s.replace(line, new, 1)


ROWS = [
    (r"^([ \t]+3[ \t]+Orange[ \t]+)", 6, "SPI MOSI", "LCD row FPC3"),
    (r"^([ \t]+4[ \t]+Green[ \t]+)", 7, "SPI CLK", "LCD row FPC4"),
    (r"^([ \t]+5[ \t]+Blue[ \t]+)", 3, "SPI CS", "LCD row FPC5"),
    (r"^([ \t]+6[ \t]+Purple[ \t]+)", 10, "Data / Command (DC)", "LCD row FPC6"),
    (r"^([ \t]+7[ \t]+Gray[ \t]+)", 11, "LCD Reset", "LCD row FPC7"),
    (r"^([ \t]+9[ \t]+Green[ \t]+)", 5, "I2C SDA", "touch row FPC9"),
    (r"^([ \t]+10[ \t]+Blue[ \t]+)", 4, "I2C SCL", "touch row FPC10"),
    (r"^([ \t]+12[ \t]+Gray[ \t]+)", 20,
     "INT (active low; IO20 cannot wake - LP_IO wake is GPIO0-7)", "touch row FPC12"),
    (r"^([ \t]+16[ \t]+\S+[ \t]+)", 19, "Touch reset", "touch row FPC16"),
]
for pref, num, label, tag in ROWS:
    rep_row(pref, num, label, tag)

DEFS = [
    ("PIN_LCD_MOSI", 6), ("PIN_LCD_CLK", 7), ("PIN_LCD_CS", 3), ("PIN_LCD_DC", 10),
    ("PIN_LCD_RST", 11), ("PIN_LCD_BL", 2), ("PIN_TOUCH_SDA", 5), ("PIN_TOUCH_SCL", 4),
    ("PIN_TOUCH_INT", 20), ("PIN_TOUCH_RST", 19), ("PIN_STATUS_LED", 15),
    ("PIN_BUTTON", 0),
]
for name, num in DEFS:
    rep_define(name, num)

old_note = "/* Deep-sleep wake capable (C6: GPIO 0-7) */"
new_note = "/* NOT a wake source: C6 LP_IO wake is GPIO0-7 only */"
if old_note in s:
    s = s.replace(old_note, new_note, 1)
else:
    fails.append("deep-sleep note not found")

# ---- alignment proof: every changed table row must keep its label column ----
import difflib
ol, nl = orig.split("\n"), s.split("\n")
shift = []
for a, b in zip(ol, nl):
    if a != b and re.match(r"^\s+\d+\s+\S+\s+\d+\s+\S", a):
        if len(a) and a.lstrip()[:1].isdigit():
            la, lb = len(a) - len(a.split()[-1]), len(b) - len(b.split()[-1])
            shift.append((la, lb, a.strip()[:28], b.strip()[:28]))
print("table rows changed        :", len(shift))
print("label column moved        :", [x for x in shift if x[0] != x[1]] or "none - all aligned")

if fails:
    print("FAILURES:", fails)
    raise SystemExit(1)

open(COPY, "wb").write(s.replace("\n", "\r\n").encode("utf-8"))
print("wrote proposed copy       :", COPY)
print()
d = subprocess.run(["diff", "-u", "--label", "a/firmware/main/config.h",
                    "--label", "b/firmware/main/config.h", SRC, COPY],
                   capture_output=True, text=True)
print("diff vs CURRENT working file (should be only the table realignment):")
print(d.stdout.replace("\r", "") or "(identical)")
