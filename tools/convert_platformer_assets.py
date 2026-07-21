#!/usr/bin/env python3
"""Convert selected sprite frames from the reference Mario project.

The reference project stores sprite-sheet coordinates and animation timing in
JSON manifests.  PGOS does not load Pygame, PNG/GIF or JSON at runtime, so this
tool converts only the requested small frames into compact RGB565 + A8 arrays.
The generated header is consumed by the native PixelSpriteRenderer; the source
project remains offline-only and is never parsed by the firmware.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Iterable

from PIL import Image


TILE_SIZE = 16

DEFAULT_MANIFESTS = (
    "Mario.json",
    "Goomba.json",
    "Koopa.json",
    "BackgroundSprites.json",
    "Animations.json",
    "ItemAnimations.json",
    "RedMushroom.json",
)

DEFAULT_WANTED = frozenset(
    {
        "mario_idle",
        "mario_run1",
        "mario_run2",
        "mario_run3",
        "mario_jump",
        "mario_big_idle",
        "mario_big_run1",
        "mario_big_run2",
        "mario_big_run3",
        "mario_big_jump",
        "goomba-1",
        "goomba-2",
        "goomba-flat",
        "koopa-1",
        "koopa-2",
        "ground",
        "bricks",
        "empty",
        "pipeL",
        "pipeR",
        "pipe2L",
        "pipe2R",
        "cloud1_1",
        "cloud1_2",
        "cloud1_3",
        "cloud2_1",
        "cloud2_2",
        "cloud2_3",
        "bush_1",
        "bush_2",
        "bush_3",
        "CoinBox_1",
        "CoinBox_2",
        "CoinBox_3",
        "coin-item_1",
        "coin-item_2",
        "coin-item_3",
        "coin-item_4",
        "mushroom",
    }
)


def safe_name(value: str) -> str:
    value = re.sub(r"[^a-zA-Z0-9_]", "_", value)
    if value and value[0].isdigit():
        value = "asset_" + value
    return value or "asset"


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red * 31 // 255) << 11) | ((green * 63 // 255) << 5) | (
        blue * 31 // 255
    )


def color_key(value, image: Image.Image):
    if value is None:
        return None
    if value == -1:
        return image.convert("RGB").getpixel((0, 0))
    if isinstance(value, list) and len(value) == 3:
        return tuple(int(channel) for channel in value)
    return None


def read_pixel_frame(
    image: Image.Image,
    rectangle: tuple[int, int, int, int],
    key,
) -> tuple[int, int, list[int], list[int]]:
    source = image.convert("RGBA").crop(rectangle)
    pixels: list[int] = []
    alpha: list[int] = []
    pixel_data = getattr(source, "get_flattened_data", source.getdata)()
    for red, green, blue, source_alpha in pixel_data:
        transparent = (
            source_alpha == 0
            or (key is not None and (red, green, blue) == key)
        )
        pixels.append(rgb565(red, green, blue))
        alpha.append(0 if transparent else source_alpha)
    return source.width, source.height, pixels, alpha


def load_manifest(source_root: Path, manifest_name: str) -> Iterable[tuple[str, int, int, int, int, object]]:
    manifest_path = source_root / "sprites" / manifest_name
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest_type = data["type"]
    default_width, default_height = data.get("size", [TILE_SIZE, TILE_SIZE])

    with Image.open(source_root / data["spriteSheetURL"]) as sheet:
        for sprite in data["sprites"]:
            name = sprite["name"]
            key = color_key(sprite.get("colorKey"), sheet)
            if manifest_type in ("background", "animation"):
                if manifest_type == "animation":
                    for index, frame in enumerate(sprite["images"]):
                        x = int(frame["x"]) * TILE_SIZE
                        y = int(frame["y"]) * TILE_SIZE
                        yield (f"{name}_{index + 1}", x, y, default_width, default_height, key)
                else:
                    x = int(sprite["x"]) * TILE_SIZE
                    y = int(sprite["y"]) * TILE_SIZE
                    yield (name, x, y, default_width, default_height, key)
            else:
                width = int(sprite.get("xsize", default_width))
                height = int(sprite.get("ysize", default_height))
                yield (name, int(sprite["x"]), int(sprite["y"]), width, height, key)


def emit_array(values: list[int], value_type: str, per_line: int = 12) -> str:
    lines = []
    for offset in range(0, len(values), per_line):
        chunk = values[offset : offset + per_line]
        if value_type == "uint16_t":
            lines.append("    " + ", ".join(f"0x{value:04X}" for value in chunk))
        else:
            lines.append("    " + ", ".join(str(value) for value in chunk))
    return ",\n".join(lines)


def packed_rgb565a8(pixels: list[int], alpha: list[int]) -> list[int]:
    packed: list[int] = []
    for pixel in pixels:
        packed.extend((pixel & 0xFF, (pixel >> 8) & 0xFF))
    packed.extend(alpha)
    return packed


def generate(
    source_root: Path,
    output_path: Path,
    manifests: Iterable[str] = DEFAULT_MANIFESTS,
    wanted: Iterable[str] = DEFAULT_WANTED,
) -> None:
    wanted = set(wanted)

    frames = []
    seen: set[str] = set()
    for manifest in manifests:
        sheet_path = source_root / json.loads(
            (source_root / "sprites" / manifest).read_text(encoding="utf-8")
        )["spriteSheetURL"]
        with Image.open(sheet_path) as sheet:
            for name, x, y, width, height, key in load_manifest(source_root, manifest):
                if name not in wanted or name in seen:
                    continue
                rectangle = (x, y, x + width, y + height)
                if x < 0 or y < 0 or x + width > sheet.width or y + height > sheet.height:
                    raise ValueError(
                        f"frame {name!r} is outside {sheet_path}: {rectangle} / {sheet.size}"
                    )
                frames.append((name, read_pixel_frame(sheet, rectangle, key)))
                seen.add(name)

    missing = sorted(wanted - seen)
    if missing:
        raise ValueError("requested frames were not found: " + ", ".join(missing))

    lines = [
        "// Generated by tools/convert_platformer_assets.py; do not edit by hand.",
        "// Source images stay outside the firmware and are never parsed at runtime.",
        "#pragma once",
        "",
        "#include <cstdint>",
        '#include "ui/PixelSprite.h"',
        "",
        "namespace pgos::platformer_assets {",
        "",
        "using Frame = pgos::PixelSprite;",
        "",
    ]
    frame_names = []
    for name, (width, height, pixels, alpha) in frames:
        symbol = safe_name(name)
        frame_names.append(symbol)
        image_data = packed_rgb565a8(pixels, alpha)
        lines.extend(
            [
                f"inline constexpr uint8_t {symbol}_image_data[] = {{",
                emit_array(image_data, "uint8_t", per_line=24),
                "};",
                f"inline constexpr Frame {symbol} = {{",
                f'    "{name}", {width}, {height},',
                f"    {symbol}_image_data, sizeof({symbol}_image_data), false",
                "};",
                "",
            ]
        )
    lines.append("inline constexpr const Frame* all[] = {")
    for symbol in frame_names:
        lines.append(f"    &{symbol},")
    lines.extend(["};", "", "}  // namespace pgos::platformer_assets", ""])
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"generated {len(frames)} frames -> {output_path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.source.resolve(), args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
