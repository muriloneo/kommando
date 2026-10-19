import pymupdf, re

src = r"C:/codes/esp32/kommando/ki-kad/1.9inch_Touch_LCD_Schematic.pdf"
page = pymupdf.open(src)[0]
words = page.get_text("words")

def near(cx, cy, rx, ry):
    out = []
    for w in words:
        x0, y0, x1, y1, t = w[0], w[1], w[2], w[3], w[4]
        if abs((x0 + x1) / 2 - cx) <= rx and abs((y0 + y1) / 2 - cy) <= ry:
            out.append((round(y0, 1), round(x0, 1), t))
    return sorted(out)

for ref, (x, y, rx, ry) in {"J1": (774.8, 95.1, 130, 120), "J2": (123.1, 147.8, 130, 120),
                            "J3": (190.9, 170.4, 130, 120)}.items():
    print("\n" + "=" * 72)
    print(f"### {ref} at ({x:.0f},{y:.0f})  (window +/-{rx},{ry})")
    for yy, xx, tt in near(x, y, rx, ry):
        print(f"   y={yy:>7} x={xx:>7}  {tt}")
