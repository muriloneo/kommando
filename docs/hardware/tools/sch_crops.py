import pymupdf

src = r"C:/codes/esp32/kommando/ki-kad/1.9inch_Touch_LCD_Schematic.pdf"
page = pymupdf.open(src)[0]

# J2 area (left) and J1 area (right) as tight crops at high DPI
crops = {
    "j2": pymupdf.Rect(20, 140, 150, 315),
    "j1": pymupdf.Rect(640, 60, 860, 300),
    "j3": pymupdf.Rect(180, 160, 300, 300),
}
for name, r in crops.items():
    pix = page.get_pixmap(clip=r, dpi=400)
    p = rf"C:/Users/Murilo/AppData/Local/Temp/sch_{name}.png"
    pix.save(p)
    print(name, p, pix.width, "x", pix.height)
