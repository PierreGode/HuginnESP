#!/usr/bin/env python3
"""Tile selected frames from tdongle_ui_sim into one upscaled PNG contact sheet.

usage: tdongle_ui_preview.py <frame_dir> <out.png> [frame_index ...]
"""
import sys
from PIL import Image

W, H, SCALE, COLS = 160, 80, 3, 3
src, out = sys.argv[1], sys.argv[2]
idx = [int(a) for a in sys.argv[3:]] or [15, 40, 80, 130, 200, 330, 420, 600, 700, 880, 1000, 1350]
tiles = []
for i in idx:
    with open(f"{src}/f{i:05d}.rgb", "rb") as f:
        tiles.append(Image.frombytes("RGB", (W, H), f.read()).resize((W * SCALE, H * SCALE), Image.NEAREST))
rows = (len(tiles) + COLS - 1) // COLS
sheet = Image.new("RGB", (COLS * (W * SCALE + 6) + 6, rows * (H * SCALE + 6) + 6), (40, 40, 48))
for n, t in enumerate(tiles):
    sheet.paste(t, (6 + (n % COLS) * (W * SCALE + 6), 6 + (n // COLS) * (H * SCALE + 6)))
sheet.save(out)
print(out)
