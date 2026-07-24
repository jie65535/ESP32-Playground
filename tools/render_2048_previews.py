#!/usr/bin/env python3
"""Render deterministic 2048 frames for offline layout review."""

from __future__ import annotations

import pathlib

from PIL import Image, ImageDraw, ImageFont


ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "captures" / "2048"
WIDTH, HEIGHT = 320, 240
BG = (17, 24, 39)
STATUS = (13, 17, 24)
BOARD = (30, 41, 59)
GAP = (51, 65, 85)
TEXT = (248, 250, 252)
MUTED = (148, 163, 184)
ACCENT = (167, 139, 250)
TILES = {
    2: (238, 232, 222), 4: (237, 224, 200), 8: (242, 177, 121),
    16: (245, 149, 99), 32: (246, 124, 95), 64: (246, 94, 59),
    128: (237, 207, 114), 256: (237, 204, 97), 512: (237, 200, 80),
    1024: (237, 197, 63), 2048: (237, 194, 46),
}


def font(size: int):
    for path in (r"C:\Windows\Fonts\segoeuib.ttf", r"C:\Windows\Fonts\arialbd.ttf"):
        if pathlib.Path(path).is_file():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def centered(draw: ImageDraw.ImageDraw, box, value: str, size: int, fill):
    left, top, right, bottom = box
    draw.text(((left + right) / 2, (top + bottom) / 2), value,
              font=font(size), fill=fill, anchor="mm")


def tile(draw: ImageDraw.ImageDraw, index: int, value: int, scale: float = 1.0,
         offset=(0, 0)):
    x = 72 + (index % 4) * 45 + offset[0]
    y = 59 + (index // 4) * 45 + offset[1]
    size = max(5, int(40 * scale))
    inset = (40 - size) // 2
    box = (x + inset, y + inset, x + inset + size - 1, y + inset + size - 1)
    color = TILES.get(value, (168, 85, 247))
    draw.rounded_rectangle(box, radius=7, fill=color)
    text_color = GAP if value <= 4 else (255, 255, 255)
    text_size = 24 if value < 100 else 20 if value < 1000 else 14 if value < 10000 else 10
    centered(draw, box, str(value), text_size, text_color)


def frame(board, score=0, best=0, overlay=None, target=False, merge=False):
    image = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 319, 21), fill=STATUS)
    draw.rectangle((0, 21, 319, 21), fill=(255, 101, 77))
    draw.text((10, 10), "PGOS", font=font(11), fill=TEXT, anchor="lm")
    draw.text((310, 10), "--:--", font=font(10), fill=MUTED, anchor="rm")
    draw.text((9, 37), "2048", font=font(28), fill=TEXT, anchor="lm")
    for left, label, value, color in ((143, "SCORE", score, TEXT),
                                      (228, "BEST", best, ACCENT)):
        draw.rounded_rectangle((left, 26, left + 78, 50), radius=5, fill=BOARD)
        draw.text((left + 39, 30), label, font=font(8), fill=MUTED, anchor="ma")
        draw.text((left + 39, 46), str(value), font=font(10), fill=color,
                  anchor="ms")
    draw.rounded_rectangle((67, 54, 251, 238), radius=10, fill=BOARD)
    for index in range(16):
        x = 72 + (index % 4) * 45
        y = 59 + (index // 4) * 45
        draw.rounded_rectangle((x, y, x + 39, y + 39), radius=7, fill=GAP)
    for index, value in enumerate(board):
        if value:
            tile(draw, index, value)
    if merge:
        tile(draw, 1, 128, offset=(13, 0))
        tile(draw, 2, 128, offset=(-13, 0))
        tile(draw, 1, 256, scale=1.12, offset=(23, 0))
    if target:
        draw.rectangle((0, 22, 319, 239), fill=(17, 24, 39))
        particles = ((18, 53, (244, 197, 66)), (42, 157, (34, 211, 238)),
                     (77, 36, (251, 113, 133)), (239, 42, (167, 139, 250)),
                     (277, 162, (244, 197, 66)), (302, 76, (34, 211, 238)))
        for x, y, color in particles:
            draw.rounded_rectangle((x - 2, y - 4, x + 2, y + 4), radius=1,
                                   fill=color)
        draw.rounded_rectangle((40, 61, 280, 197), radius=12,
                               fill=(244, 197, 66))
        draw.rounded_rectangle((42, 63, 278, 195), radius=10, fill=(23, 27, 38))
        centered(draw, (60, 69, 260, 86), "TILE UNLOCKED", 11, (244, 197, 66))
        centered(draw, (60, 87, 260, 122), "2048", 28, TEXT)
        draw.rounded_rectangle((114, 126, 206, 128), radius=1,
                               fill=(244, 197, 66))
        centered(draw, (60, 133, 260, 158), "YOU MADE IT", 20, TEXT)
        centered(draw, (60, 163, 260, 180), "KEEP BUILDING", 10, MUTED)
    if overlay:
        title, subtitle = overlay
        draw.rounded_rectangle((30, 97, 290, 165), radius=12, fill=BG)
        centered(draw, (30, 105, 290, 132), title, 20, TEXT)
        centered(draw, (30, 136, 290, 159), subtitle, 11, MUTED)
    return image


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    frames = {
        "title.png": frame([0] * 16, overlay=("BUILD 2048", "PRESS A TO START")),
        "running.png": frame([2, 4, 8, 16, 32, 64, 128, 0,
                              4, 8, 16, 0, 2, 4, 0, 0], 1356, 8840),
        "merge.png": frame([2, 0, 0, 16, 32, 64, 0, 0,
                            4, 8, 16, 0, 2, 4, 0, 0], 1612, 8840, merge=True),
        "target.png": frame([2048, 512, 128, 32, 256, 64, 16, 8,
                             32, 8, 4, 2, 4, 2, 0, 0], 20148, 20148,
                            target=True),
        "game_over.png": frame([2, 4, 2, 4, 4, 2, 4, 2,
                                2, 4, 8, 16, 4, 2, 16, 8], 16840, 20148,
                               overlay=("NO MOVES", "SCORE 16840   A RESTART")),
    }
    for name, image in frames.items():
        image.save(OUTPUT / name)
    sheet = Image.new("RGB", (WIDTH * 3, HEIGHT * 2), (8, 11, 16))
    for index, image in enumerate(frames.values()):
        sheet.paste(image, ((index % 3) * WIDTH, (index // 3) * HEIGHT))
    sheet.save(OUTPUT / "contact_sheet.png")
    print(f"wrote {len(frames)} previews and contact sheet to {OUTPUT}")


if __name__ == "__main__":
    main()
