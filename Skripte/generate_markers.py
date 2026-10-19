"""Erzeugt druckfertige ArUco-Marker für alle Felder des Labyrinths.

Pro Feld ein Marker aus dem Wörterbuch DICT_4X4_50 (IDs 0 bis 49).
Zuordnung:  ID = x + y * MAZE_W   (x = 0..MAZE_W-1, y = 0..MAZE_H-1)

Ausgabe im Ordner aruco_marker/:
    aruco_marker.pdf   alle Marker, A4, in Originalgröße (Drucker auf 100 % /
                       "Tatsächliche Größe" stellen, nicht "An Seite anpassen")
    marker_XX.png      jeder Marker einzeln

Abhängigkeiten:
    opencv-contrib-python, Pillow
"""
import os

import cv2
from PIL import Image, ImageDraw, ImageFont

# ---- Parameter (müssen zu ar_test.py und dem Spiel passen) ----
MARKER_SIZE_MM = 50     # Kantenlänge des schwarzen Markers, = MARKER_SIZE in ar_test.py (0.05 m)
MAZE_W = 10             # wie in MicromouseMapping.h
MAZE_H = 5
DPI = 300

# ---- Seitenlayout (A4) ----
PAGE_W_MM, PAGE_H_MM = 210, 297
CELL_MM = 66            # Platz pro Marker inkl. weißem Rand (Mindestens ~1 Modul = 8 mm um den Marker)
COLS, ROWS = 3, 4

OUT_DIR = "aruco_marker"


def mm_to_px(mm):
    return round(mm / 25.4 * DPI)


def make_marker(marker_id):
    """Der Marker allein als Graustufenbild (0 = schwarz, 255 = weiß)."""
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)
    side = mm_to_px(MARKER_SIZE_MM)
    return cv2.aruco.generateImageMarker(dictionary, marker_id, side)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    n_markers = MAZE_W * MAZE_H
    if n_markers > 50:
        raise ValueError("DICT_4X4_50 hat nur 50 Marker. Größeres Wörterbuch wählen.")

    font = ImageFont.load_default(size=mm_to_px(3.5))
    page_w, page_h = mm_to_px(PAGE_W_MM), mm_to_px(PAGE_H_MM)
    cell = mm_to_px(CELL_MM)
    marker_px = mm_to_px(MARKER_SIZE_MM)
    x0 = (page_w - COLS * cell) // 2           # Raster auf der Seite zentrieren
    y0 = (page_h - ROWS * cell) // 2

    per_page = COLS * ROWS
    pages = []
    for start in range(0, n_markers, per_page):
        page = Image.new("L", (page_w, page_h), 255)
        draw = ImageDraw.Draw(page)
        for slot, marker_id in enumerate(range(start, min(start + per_page, n_markers))):
            col, row = slot % COLS, slot // COLS
            cx, cy = x0 + col * cell, y0 + row * cell

            # Schnittlinie um die Zelle (hellgrau, stört die Erkennung nicht)
            draw.rectangle([cx, cy, cx + cell, cy + cell], outline=200, width=2)

            marker = Image.fromarray(make_marker(marker_id))
            off = (cell - marker_px) // 2
            page.paste(marker, (cx + off, cy + off))

            x, y = marker_id % MAZE_W, marker_id // MAZE_W
            draw.text((cx + mm_to_px(2), cy + mm_to_px(1)),
                      f"ID {marker_id}  (x={x}, y={y})", fill=120, font=font)

            # Einzeldatei mit weißem Rand
            single = Image.new("L", (cell, cell), 255)
            single.paste(marker, (off, off))
            single.save(os.path.join(OUT_DIR, f"marker_{marker_id:02d}.png"), dpi=(DPI, DPI))
        pages.append(page)

    pdf_path = os.path.join(OUT_DIR, "aruco_marker.pdf")
    pages[0].save(pdf_path, save_all=True, append_images=pages[1:], resolution=DPI)
    print(f"{n_markers} Marker, {len(pages)} Seiten -> {pdf_path}")


if __name__ == "__main__":
    main()
