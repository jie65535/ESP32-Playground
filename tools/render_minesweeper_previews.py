#!/usr/bin/env python3
"""Render deterministic Minesweeper style frames for offline layout review."""

from __future__ import annotations

import pathlib
from PIL import Image, ImageDraw, ImageFont


ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "captures" / "minesweeper"
WIDTH, HEIGHT = 320, 240
BG = (20, 24, 26)
HEADER = (32, 38, 41)
FRAME = (9, 11, 12)
COVERED = (122, 130, 131)
HIGHLIGHT = (198, 205, 203)
SHADOW = (55, 61, 63)
REVEALED = (184, 185, 180)
GRID = (138, 142, 140)
AMBER = (242, 184, 75)
RED = (223, 77, 77)
GREEN = (66, 185, 130)
NUMBERS = [
    (37, 89, 199),
    (39, 131, 79),
    (198, 62, 62),
    (90, 61, 157),
    (138, 51, 45),
    (24, 125, 134),
    (34, 38, 41),
    (109, 116, 117),
]


def font(size: int):
    for path in (r"C:\Windows\Fonts\msyh.ttc", r"C:\Windows\Fonts\segoeui.ttf"):
        if pathlib.Path(path).is_file():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def text(draw: ImageDraw.ImageDraw, xy, value: str, size: int, fill, anchor="mm"):
    draw.text(xy, value, font=font(size), fill=fill, anchor=anchor)


def mine(draw, box, color=AMBER, detonated=False):
    x1, y1, x2, y2 = box
    cx, cy = (x1 + x2) // 2, (y1 + y2) // 2
    unit = max(1, min(x2 - x1 + 1, y2 - y1 + 1) // 9)
    draw.rectangle((cx - 4 * unit, cy - unit, cx + 4 * unit, cy + unit), fill=color)
    draw.rectangle((cx - unit, cy - 4 * unit, cx + unit, cy + 4 * unit), fill=color)
    draw.ellipse((cx - 3 * unit, cy - 3 * unit, cx + 3 * unit, cy + 3 * unit), fill=color)
    if detonated:
        draw.rectangle((cx - unit, cy - unit, cx + unit, cy + unit), fill=(244, 245, 242))


def flag(draw, box, color=(232, 76, 76)):
    x1, y1, x2, y2 = box
    cx = (x1 + x2) // 2
    draw.rectangle((cx, y1 + 2, cx + 1, y2 - 2), fill=FRAME)
    draw.rectangle((cx - 4, y2 - 3, cx + 4, y2 - 1), fill=FRAME)
    draw.polygon(((cx, y1 + 2), (cx - 5, y1 + 7), (cx, y1 + 10)), fill=color)


def board_frame(draw, x, y, cell, cols, rows, phase="playing"):
    draw.rectangle((x - 2, y - 2, x + cols * cell + 1, y + rows * cell + 1), fill=FRAME)
    # A deterministic field with a readable mix of zeroes, numbers, flags and mines.
    mines = {(2, 2), (5, 1), (7, 4), (1, 7), (8, 6)}
    flags = {(2, 2), (5, 1)}
    revealed = {(xx, yy) for yy in range(rows) for xx in range(cols) if yy < rows // 2}
    for yy in range(rows):
        for xx in range(cols):
            left, top = x + xx * cell, y + yy * cell
            tile = (left, top, left + cell - 1, top + cell - 1)
            is_mine = (xx, yy) in mines
            if phase == "lost" and is_mine:
                draw.rectangle(tile, fill=RED if (xx, yy) == (7, 4) else REVEALED)
                mine(draw, (left + 1, top + 1, left + cell - 2, top + cell - 2), FRAME, (xx, yy) == (7, 4))
                continue
            if phase == "won" and is_mine:
                draw.rectangle(tile, fill=COVERED)
                draw.line((left, top, left + cell - 1, top + cell - 1), fill=HIGHLIGHT)
                flag(draw, (left + 1, top + 1, left + cell - 2, top + cell - 2))
                continue
            if (xx, yy) in flags and phase == "playing":
                draw.rectangle(tile, fill=COVERED)
                draw.line((left, top, left + cell - 1, top), fill=HIGHLIGHT)
                draw.line((left, top, left, top + cell - 1), fill=HIGHLIGHT)
                draw.line((left, top + cell - 1, left + cell - 1, top + cell - 1), fill=SHADOW)
                flag(draw, (left + 1, top + 1, left + cell - 2, top + cell - 2))
                continue
            if (xx, yy) not in revealed:
                draw.rectangle(tile, fill=COVERED)
                draw.line((left, top, left + cell - 1, top), fill=HIGHLIGHT)
                draw.line((left, top, left, top + cell - 1), fill=HIGHLIGHT)
                draw.line((left, top + cell - 1, left + cell - 1, top + cell - 1), fill=SHADOW)
                draw.line((left + cell - 1, top, left + cell - 1, top + cell - 1), fill=SHADOW)
                continue
            draw.rectangle(tile, fill=GRID)
            draw.rectangle((left + 1, top + 1, left + cell - 2, top + cell - 2), fill=REVEALED)
            adjacent = sum((xx + dx, yy + dy) in mines for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy)
            if adjacent:
                text(draw, (left + cell / 2, top + cell / 2), str(adjacent), max(7, cell - 5), NUMBERS[adjacent - 1])
    if phase == "playing":
        cursor_x, cursor_y = cols // 2, rows // 2
        left, top = x + cursor_x * cell, y + cursor_y * cell
        tile = (left, top, left + cell - 1, top + cell - 1)
        draw.rectangle(tile, outline=(255, 240, 166), width=2 if cell >= 15 else 1)


def status_bar(image):
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, WIDTH, 21), fill=(13, 17, 24))
    draw.rectangle((0, 21, WIDTH, 21), fill=(255, 101, 77))
    text(draw, (11, 10), "PGOS", 12, (245, 247, 250))
    text(draw, (303, 10), "--:--", 11, (154, 165, 181), anchor="rm")


def chrome(image, remaining, timer):
    status_bar(image)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 22, WIDTH, 55), fill=HEADER)
    draw.rectangle((8, 27, 63, 51), fill=FRAME)
    text(draw, (35, 39), f"{remaining:03d}", 13, RED)
    draw.rectangle((256, 27, 312, 51), fill=FRAME)
    text(draw, (284, 39), f"{timer:03d}", 13, AMBER)
    mine(draw, (146, 29, 174, 50), AMBER)


def render_game(name, cols, rows, cell, phase="playing"):
    image = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)
    chrome(image, 28 if phase == "playing" else 0, 47 if phase == "playing" else 92)
    x = (WIDTH - cols * cell) // 2
    y = 60 + (174 - rows * cell) // 2
    board_frame(draw, x, y, cell, cols, rows, phase)
    if phase == "paused":
        draw.rectangle((0, 56, 319, 239), fill=FRAME)
        text(draw, (160, 133), "PAUSED", 22, (244, 245, 242))
        text(draw, (160, 163), "游戏已暂停", 12, (159, 169, 168))
    elif phase == "generating":
        draw.rounded_rectangle((88, 110, 232, 154), radius=5, fill=FRAME)
        text(draw, (160, 132), "LOGIC CHECK  12", 11, AMBER)
    elif phase in ("won", "lost"):
        draw.rectangle((22, 112, 298, 229), fill=(32, 38, 41))
        text(draw, (160, 133), "FIELD CLEARED" if phase == "won" else "MINE TRIGGERED", 20, GREEN if phase == "won" else RED)
        text(draw, (160, 160), "047.2 秒 / NEW BEST" if phase == "won" else "092.0 秒", 12, (244, 245, 242))
        for index, label in enumerate(("再来一局", "难度")):
            left = 40 + index * 123
            draw.rectangle((left, 187, left + 118, 219), fill=AMBER if index == 0 else FRAME)
            text(draw, (left + 59, 203), label, 12, HEADER if index == 0 else (244, 245, 242))
    return image


def render_custom():
    image = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)
    status_bar(image)
    draw.rectangle((0, 22, WIDTH, 52), fill=HEADER)
    text(draw, (10, 37), "CUSTOM FIELD", 12, (244, 245, 242), anchor="lm")
    labels = (("宽度", "12"), ("高度", "12"), ("地雷", "24"), ("开始", "12x12 / 24"))
    for row, (label, value) in enumerate(labels):
        top = 68 + row * 39
        selected = row == 2
        if selected:
            draw.rounded_rectangle((34, top, 286, top + 32), radius=4, fill=(51, 46, 37))
            draw.rectangle((34, top, 37, top + 32), fill=AMBER)
        text(draw, (46, top + 16), label, 12, (244, 245, 242) if selected else (159, 169, 168), anchor="lm")
        text(draw, (274, top + 16), value, 12, AMBER if selected else (244, 245, 242), anchor="rm")
    return image


def render_records():
    image = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)
    status_bar(image)
    draw.rectangle((0, 22, WIDTH, 52), fill=HEADER)
    text(draw, (10, 37), "RECORDS", 12, (244, 245, 242), anchor="lm")
    rows = (("初级", "BEST 042.7   8/12   STREAK 3"),
            ("中级", "BEST 118.2   3/9    STREAK 2"),
            ("专家", "BEST ---     0/4    STREAK 0"))
    for row, (label, value) in enumerate(rows):
        top = 64 + row * 50
        draw.rounded_rectangle((14, top, 306, top + 42), radius=3, fill=(27, 32, 34) if row % 2 == 0 else HEADER)
        text(draw, (24, top + 21), label, 12, (244, 245, 242), anchor="lm")
        text(draw, (296, top + 21), value, 9, (159, 169, 168), anchor="rm")
    text(draw, (160, 224), "自定义   2 / 5", 11, (159, 169, 168))
    return image


def render_title():
    image = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)
    status_bar(image)
    draw.rectangle((0, 22, WIDTH, 49), fill=HEADER)
    text(draw, (9, 36), "MINESWEEPER", 12, (244, 245, 242), anchor="lm")
    mine(draw, (139, 64, 181, 106), AMBER)
    text(draw, (160, 119), "MINESWEEPER", 22, (244, 245, 242))
    text(draw, (160, 143), "PGOS LOGIC FIELD", 10, (159, 169, 168))
    for index, label in enumerate(("初级", "中级", "专家", "自定义")):
        left = 6 + index * 78
        selected = index == 1
        draw.rounded_rectangle((left, 154, left + 72, 197), radius=5, fill=AMBER if selected else FRAME)
        draw.rounded_rectangle((left + 2, 156, left + 70, 195), radius=3, fill=(52, 48, 42) if selected else HEADER)
        text(draw, (left + 36, 171), label, 12, AMBER if selected else (244, 245, 242))
        text(draw, (left + 36, 187), ("9x9 / 10", "16x16 / 40", "30x16 / 99", "CUSTOM")[index], 8, (159, 169, 168))
    draw.rounded_rectangle((62, 201, 258, 229), radius=4, fill=FRAME)
    draw.rounded_rectangle((64, 203, 256, 227), radius=3, fill=HEADER)
    text(draw, (72, 215), "RECORDS", 9, (244, 245, 242), anchor="lm")
    text(draw, (248, 215), "42.7 s / 8/12", 9, (159, 169, 168), anchor="rm")
    return image


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    frames = {
        "title.png": render_title(),
        "beginner.png": render_game("MINESWEEPER", 9, 9, 18),
        "expert.png": render_game("MINESWEEPER", 30, 16, 10),
        "custom.png": render_custom(),
        "records.png": render_records(),
        "paused.png": render_game("MINESWEEPER", 16, 16, 11, "paused"),
        "generating.png": render_game("MINESWEEPER", 16, 16, 11, "generating"),
        "won.png": render_game("MINESWEEPER", 16, 16, 11, "won"),
        "lost.png": render_game("MINESWEEPER", 16, 16, 11, "lost"),
    }
    for name, image in frames.items():
        image.save(OUTPUT / name)
    sheet = Image.new("RGB", (WIDTH * 3, HEIGHT * 3), (8, 11, 16))
    for index, image in enumerate(frames.values()):
        sheet.paste(image, ((index % 3) * WIDTH, (index // 3) * HEIGHT))
    sheet.save(OUTPUT / "contact_sheet.png")
    print(f"wrote {len(frames)} previews and contact sheet to {OUTPUT}")


if __name__ == "__main__":
    main()
