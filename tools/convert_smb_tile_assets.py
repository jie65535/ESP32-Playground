#!/usr/bin/env python3
"""Compile the complete SMB fan-project sprite sheets into palette tiles."""

from __future__ import annotations

import argparse
import hashlib
import math
import re
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


DEFAULT_SOURCE = Path("reference/Super-Mario-Bros/res")
DEFAULT_OUTPUT = Path("src/games/PlatformerTileAssets.generated.cpp")
TRANSPARENT = (147, 187, 236, 255)


@dataclass(frozen=True)
class SheetSpec:
    name: str
    relative_path: str
    columns: int
    rows: int
    x_offset: int
    y_offset: int
    gap: int


SHEETS = (
    SheetSpec("BLOCK", "sprites/blocks/BlockTileSheet.png", 48, 22, 1, 1, 1),
    SheetSpec("ENEMY", "sprites/characters/EnemySpriteSheet.png", 35, 15, 1, 1, 0),
    SheetSpec("PLAYER", "sprites/characters/PlayerSpriteSheet.png", 25, 16, 1, 9, 0),
)


@dataclass(frozen=True)
class EncodedSheet:
    spec: SheetSpec
    palette: tuple[int, ...]
    bits_per_pixel: int
    pixels: bytes


def rgb565(color: tuple[int, int, int, int]) -> int:
    red, green, blue, _ = color
    return ((red * 31 // 255) << 11) | ((green * 63 // 255) << 5) | (
        blue * 31 // 255
    )


def pack_indices(indices: list[int], bits: int) -> bytes:
    output = bytearray((len(indices) * bits + 7) // 8)
    for position, value in enumerate(indices):
        bit = position * bits
        output[bit >> 3] |= (value << (bit & 7)) & 0xFF
        if (bit & 7) + bits > 8:
            output[(bit >> 3) + 1] |= value >> (8 - (bit & 7))
    return bytes(output)


def unpack_index(data: bytes, position: int, bits: int) -> int:
    bit = position * bits
    packed = data[bit >> 3]
    if (bit & 7) + bits > 8:
        packed |= data[(bit >> 3) + 1] << 8
    return (packed >> (bit & 7)) & ((1 << bits) - 1)


def encode_sheet(source: Path, spec: SheetSpec) -> EncodedSheet:
    with Image.open(source / spec.relative_path) as opened:
        image = opened.convert("RGBA")
    tiles: list[list[tuple[int, int, int, int]]] = []
    colors: set[tuple[int, int, int, int]] = set()
    flat_data = getattr(image, "get_flattened_data", image.getdata)
    del flat_data  # Crops below avoid including labels outside the tile grids.
    for tile_id in range(spec.columns * spec.rows):
        column = tile_id % spec.columns
        row = tile_id // spec.columns
        x = spec.x_offset + column * (16 + spec.gap)
        y = spec.y_offset + row * (16 + spec.gap)
        if x + 16 > image.width or y + 16 > image.height:
            raise ValueError(f"{spec.name} tile {tile_id} exceeds {image.size}")
        tile = image.crop((x, y, x + 16, y + 16))
        pixels = list(getattr(tile, "get_flattened_data", tile.getdata)())
        tiles.append(pixels)
        colors.update(color for color in pixels if color != TRANSPARENT and color[3] != 0)

    ordered = sorted(colors)
    palette = (0,) + tuple(rgb565(color) for color in ordered)
    bits = max(1, math.ceil(math.log2(len(palette))))
    if bits > 5:
        raise ValueError(f"{spec.name} needs {bits} bits for {len(palette)} colors")
    lookup = {color: index + 1 for index, color in enumerate(ordered)}
    indices = [
        0 if color == TRANSPARENT or color[3] == 0 else lookup[color]
        for tile in tiles
        for color in tile
    ]
    packed = pack_indices(indices, bits)
    if any(unpack_index(packed, index, bits) != value for index, value in enumerate(indices)):
        raise ValueError(f"{spec.name} bit packing did not round-trip")
    return EncodedSheet(spec, palette, bits, packed)


def block_references(source: Path) -> list[int]:
    irregular: dict[int, int] = {}
    blockmap = source / "sprites/blocks/IrregularReferences.blockmap"
    for match in re.finditer(r"(\d+),\s*(\d+)", blockmap.read_text(encoding="utf-8")):
        irregular[int(match.group(1))] = int(match.group(2))
    references: list[int] = []
    for tile_id in range(48 * 22):
        if tile_id in irregular:
            references.append(irregular[tile_id])
            continue
        x = tile_id % 48
        source_y = tile_id // 48
        y = source_y
        if y > 10 and x < 32:
            y -= 11
        if 15 < x < 32:
            x -= 16
        elif x >= 32 and source_y < 10:
            x -= 32
        references.append(y * 48 + x)
    return references


def enemy_references() -> list[int]:
    references: list[int] = []
    for tile_id in range(35 * 15):
        x = tile_id % 35
        y = tile_id // 35
        if 2 < y < 12:
            y %= 3
        references.append(y * 35 + x)
    return references


def emit_array(values, per_line: int = 20) -> list[str]:
    values = list(values)
    return [
        "    " + ", ".join(str(value) for value in values[index : index + per_line]) + ","
        for index in range(0, len(values), per_line)
    ]


def generate(source: Path) -> str:
    sheets = [encode_sheet(source, spec) for spec in SHEETS]
    digest = hashlib.sha256()
    for spec in SHEETS:
        digest.update((source / spec.relative_path).read_bytes())
    digest.update((source / "sprites/blocks/IrregularReferences.blockmap").read_bytes())

    lines = [
        "// Generated by tools/convert_smb_tile_assets.py. Do not edit.",
        f"// Source assets SHA-256: {digest.hexdigest()}",
        '#include "games/PlatformerTileAssets.h"',
        "",
        "namespace pgos {",
        "namespace {",
        "",
    ]
    for sheet in sheets:
        lines.append(f"const uint16_t {sheet.spec.name}_PALETTE[] = {{")
        lines.extend(emit_array(sheet.palette, 12))
        lines.extend(["};", f"const uint8_t {sheet.spec.name}_PIXELS[] = {{"])
        lines.extend(emit_array(sheet.pixels, 24))
        lines.extend(["};", ""])
    lines.extend(["}  // namespace", ""])

    for sheet in sheets:
        tile_count = sheet.spec.columns * sheet.spec.rows
        lines.extend(
            [
                f"const PlatformerPackedTileSheet PLATFORMER_{sheet.spec.name}_TILES = {{",
                f"    {sheet.spec.name}_PALETTE, {sheet.spec.name}_PIXELS,",
                f"    sizeof({sheet.spec.name}_PIXELS), {tile_count},",
                f"    {len(sheet.palette)}, {sheet.bits_per_pixel},",
                "};",
                "",
            ]
        )
    lines.append(
        "const uint16_t PLATFORMER_BLOCK_REFERENCE_IDS[PLATFORMER_BLOCK_TILE_COUNT] = {"
    )
    lines.extend(emit_array(block_references(source)))
    lines.extend(["};", ""])
    lines.append(
        "const uint16_t PLATFORMER_ENEMY_REFERENCE_IDS[PLATFORMER_ENEMY_TILE_COUNT] = {"
    )
    lines.extend(emit_array(enemy_references()))
    lines.extend(["};", "", "}  // namespace pgos", ""])
    return "\n".join(lines)


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    content = generate(args.source)
    changed = write_if_changed(args.output, content)
    sheets = [encode_sheet(args.source, spec) for spec in SHEETS]
    packed = sum(len(sheet.pixels) for sheet in sheets)
    print(
        f"{'generated' if changed else 'unchanged'} {len(sheets)} sheets, "
        f"{packed} packed pixel bytes -> {args.output}"
    )


if __name__ == "__main__":
    main()
