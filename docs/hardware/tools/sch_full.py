import pymupdf, re

src = r"C:/codes/esp32/kommando/ki-kad/1.9inch_Touch_LCD_Schematic.pdf"
page = pymupdf.open(src)[0]
words = page.get_text("words")
items = [{"x": w[0], "y": w[1], "t": w[4]} for w in words]

def col(x0, x1, y0, y1):
    return sorted([i for i in items if x0 <= i["x"] <= x1 and y0 <= i["y"] <= y1], key=lambda i: (i["y"], i["x"]))

# all text outside the connector bands -> net labels / title text
print("=== ALL text with x < 700 (left part of sheet) ===")
for i in col(0, 700, 0, 700):
    if not i["t"].startswith(("PI", "CO", "NL")):
        print(f"  x={i['x']:>6.0f} y={i['y']:>6.0f}  {i['t']}")

print("\n=== ALL text with x > 1000 (right part) ===")
for i in col(1000, 900, 0, 700):
    if not i["t"].startswith(("PI", "CO", "NL")):
        print(f"  x={i['x']:>6.0f} y={i['y']:>6.0f}  {i['t']}")

print("\n=== J1 right-side two columns (pin numbers inside symbol + labels left) ===")
for i in sorted([i for i in items if 690 <= i["x"] <= 900], key=lambda i: (round(i["y"]), i["x"])):
    if not i["t"].startswith(("PI", "CO", "NL")):
        print(f"  y={i['y']:>7.1f} x={i['x']:>7.1f}  {i['t']}")
