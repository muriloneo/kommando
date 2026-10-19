import pymupdf

src = r"C:/codes/esp32/kommando/ki-kad/1.9inch_Touch_LCD_Schematic.pdf"
doc = pymupdf.open(src)
page = doc[0]
words = page.get_text("words")          # x0,y0,x1,y1,word,block,line,wordno
print("word count:", len(words))

# locate reference designators like J1 / J2 / U1 ... and dump a neighbourhood of text
import re
refs = [(w[0], w[1], w[4]) for w in words if re.fullmatch(r"(J|U|CN|CON|P)\d{1,2}", w[4])]
print("refs found:", refs)

def near(cx, cy, rx=90, ry=70):
    out = []
    for w in words:
        x0, y0, x1, y1, t = w[0], w[1], w[2], w[3], w[4]
        if abs((x0 + x1) / 2 - cx) <= rx and abs((y0 + y1) / 2 - cy) <= ry:
            out.append((round(y0, 1), round(x0, 1), t))
    return sorted(out)

for (x, y, t) in refs:
    print("\n" + "=" * 70)
    print(f"### neighbourhood of {t} at ({x:.0f},{y:.0f})")
    for yy, xx, tt in near(x, y):
        print(f"   y={yy:>7} x={xx:>7}  {tt}")
