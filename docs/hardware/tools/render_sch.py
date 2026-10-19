import fitz

src = r"C:/codes/esp32/kommando/ki-kad/1.9inch_Touch_LCD_Schematic.pdf"
d = fitz.open(src)
print("pages:", d.page_count)
for i, page in enumerate(d):
    print(f"page {i}: {page.rect.width:.0f} x {page.rect.height:.0f} pt")
    pix = page.get_pixmap(dpi=200)
    p = rf"C:/Users/Murilo/AppData/Local/Temp/lcd_sch_p{i}.png"
    pix.save(p)
    print("  saved", p, pix.width, "x", pix.height)
