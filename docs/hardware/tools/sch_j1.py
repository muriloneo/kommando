import pymupdf

src = r"C:/codes/esp32/kommando/ki-kad/1.9inch_Touch_LCD_Schematic.pdf"
page = pymupdf.open(src)[0]
words = page.get_text("words")
items = [{"x": round(w[0], 1), "y": round(w[1], 1), "t": w[4]} for w in words]

print("=== J1 region (x 640-900) ===")
for i in sorted([i for i in items if 640 <= i["x"] <= 900], key=lambda i: (i["y"], i["x"])):
    if not i["t"].startswith(("PI", "CO", "NL")):
        print(f"  y={i['y']:>7.1f} x={i['x']:>7.1f}  {i['t']}")

print("\n=== J2 region (x 20-140) ===")
for i in sorted([i for i in items if 20 <= i["x"] <= 140], key=lambda i: (i["y"], i["x"])):
    if not i["t"].startswith(("PI", "CO", "NL")):
        print(f"  y={i['y']:>7.1f} x={i['x']:>7.1f}  {i['t']}")

print("\n=== J3 region (x 185-260) ===")
for i in sorted([i for i in items if 185 <= i["x"] <= 260], key=lambda i: (i["y"], i["x"])):
    if not i["t"].startswith(("PI", "CO", "NL")):
        print(f"  y={i['y']:>7.1f} x={i['x']:>7.1f}  {i['t']}")
