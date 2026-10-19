#!/usr/bin/env python
"""v4: realign ONLY the 9 doc-table rows in the patched config.h.

Takes the geometry (label column + gap) from the committed HEAD version and the
new numbers from the current working file, so the label column cannot move.
"""
import re
import subprocess

REPO = r"C:\codes\esp32\kommando"
SRC = REPO + r"\firmware\main\config.h"
COPY = r"C:\Users\Murilo\AppData\Local\Temp\kommando_config_patch\config.h"

ROW = re.compile(r"^(?P<before>[ \t]+\d+[ \t]+\S+[ \t]+)(?P<num>\d+)"
                 r"(?P<gap>[ \t]+)(?P<label>\S.*)$", re.M)


def rows(text):
    out = {}
    for m in ROW.finditer(text):
        key = re.match(r"[ \t]+(\d+)", m.group("before")).group(1)
        out[key] = m
    return out


orig = subprocess.run(["git", "-C", REPO, "show", "HEAD:firmware/main/config.h"],
                      capture_output=True).stdout.replace(b"\r\n", b"\n").decode()
cur = open(SRC, "rb").read().replace(b"\r\n", b"\n").decode()

ro, rc = rows(orig), rows(cur)
print("table rows found -- HEAD:", sorted(ro, key=int), "| working:", sorted(rc, key=int))

changed = []
s = cur
for key in sorted(ro, key=int):
    a, b = ro[key], rc.get(key)
    if not b:
        print("MISSING in working file:", key)
        continue
    if a.group("num") == b.group("num") and a.group("label") == b.group("label"):
        continue  # row 8: nothing to do
    num_new = b.group("num")
    before = a.group("before")
    delta = len(num_new) - len(a.group("num"))
    if delta > 0:
        before = before[:-delta]
    elif delta < 0:
        before = before + " " * (-delta)
    new_line = before + num_new + a.group("gap") + b.group("label")
    s = s.replace(b.group(0), new_line, 1)
    col_old = len(a.group("before")) + len(a.group("num")) + len(a.group("gap"))
    col_new = len(before) + len(num_new) + len(a.group("gap"))
    changed.append((key, a.group("num"), num_new, col_old, col_new))

print()
print("row | old GPIO -> new | label col old -> new")
for k, o, n, c1, c2 in changed:
    print("  %-3s      %-2s -> %-2s       %-3d -> %-3d  %s" %
          (k, o, n, c1, c2, "OK" if c1 == c2 else "*** MOVED ***"))
bad = [c for c in changed if c[3] != c[4]]
print()
print("rows changed:", len(changed), "| label columns moved:", bad or "none")

open(COPY, "wb").write(s.replace("\n", "\r\n").encode("utf-8"))
print("proposed copy written:", COPY)
print()
d = subprocess.run(["diff", "-u", "--label", "a/firmware/main/config.h",
                    "--label", "b/firmware/main/config.h", SRC, COPY],
                   capture_output=True, text=True)
print("=== diff vs current working file (should be pure table realignment) ===")
print(d.stdout.replace("\r", "") or "(identical -- nothing to realign)")
