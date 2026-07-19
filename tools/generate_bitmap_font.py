#!/usr/bin/env python3
"""Generate the Playground baseline-aligned bitmap font subset."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


BITMAP_ROWS = 20
BITMAP_BYTES = BITMAP_ROWS * 2
CPP_SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}


def extract_cpp_string_characters(text: str) -> set[str]:
    """Collect literal Unicode characters from C/C++ strings, excluding comments."""
    characters: set[str] = set()
    index = 0
    length = len(text)

    while index < length:
        if text.startswith("//", index):
            newline = text.find("\n", index + 2)
            index = length if newline < 0 else newline + 1
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = length if end < 0 else end + 2
            continue

        quote = text[index]
        if quote not in {'"', "'"}:
            index += 1
            continue

        is_string = quote == '"'
        index += 1
        while index < length:
            character = text[index]
            if character == "\\":
                if index + 1 >= length:
                    index += 1
                    break
                escape = text[index + 1]
                if is_string and escape in {"u", "U"}:
                    digits = 4 if escape == "u" else 8
                    encoded = text[index + 2:index + 2 + digits]
                    if len(encoded) == digits and re.fullmatch(
                        rf"[0-9A-Fa-f]{{{digits}}}", encoded
                    ):
                        value = chr(int(encoded, 16))
                        if not value.isspace():
                            characters.add(value)
                        index += 2 + digits
                        continue
                index += 2
                continue
            if character == quote:
                index += 1
                break
            if is_string and not character.isspace():
                characters.add(character)
            index += 1

    return characters


def collect_source_characters(source_root: Path) -> set[str]:
    characters: set[str] = set()
    if not source_root.exists():
        return characters

    for path in sorted(source_root.rglob("*")):
        if (
            not path.is_file()
            or path.suffix.lower() not in CPP_SOURCE_SUFFIXES
            or path.name == "BitmapFontData.h"
        ):
            continue
        characters.update(
            extract_cpp_string_characters(path.read_text(encoding="utf-8"))
        )
    return characters


def collect_characters(path: Path, source_root: Path | None = Path("src")) -> list[str]:
    text = path.read_text(encoding="utf-8")
    characters = {chr(value) for value in range(0x20, 0x7F)}
    characters.update(char for char in text if not char.isspace())
    if source_root is not None:
        characters.update(collect_source_characters(source_root))
    return sorted(characters, key=ord)


def parse_bdf(path: Path) -> tuple[int, dict[int, dict[str, object]]]:
    ascent: int | None = None
    glyphs: dict[int, dict[str, object]] = {}
    current: dict[str, object] | None = None
    reading_bitmap = False

    with path.open(encoding="ascii", errors="strict") as source:
        for raw_line in source:
            line = raw_line.strip()
            if line.startswith("FONT_ASCENT "):
                ascent = int(line.split()[1])
            elif line == "STARTCHAR" or line.startswith("STARTCHAR "):
                current = {"bitmap": []}
                reading_bitmap = False
            elif current is None:
                continue
            elif line.startswith("ENCODING "):
                current["encoding"] = int(line.split()[1])
            elif line.startswith("DWIDTH "):
                current["advance"] = int(line.split()[1])
            elif line.startswith("BBX "):
                current["bbx"] = tuple(int(value) for value in line.split()[1:5])
            elif line == "BITMAP":
                reading_bitmap = True
            elif line == "ENDCHAR":
                encoding = int(current.get("encoding", -1))
                if encoding >= 0:
                    glyphs[encoding] = current
                current = None
                reading_bitmap = False
            elif reading_bitmap:
                bitmap = current["bitmap"]
                assert isinstance(bitmap, list)
                bitmap.append(line)

    if ascent is None:
        raise ValueError(f"FONT_ASCENT is missing from {path}")
    return ascent, glyphs


def render_bdf_glyph(
    glyph: dict[str, object], ascent: int, canvas_width: int = 16
) -> tuple[int, bytes]:
    width, height, x_offset, y_offset = glyph["bbx"]
    advance = int(glyph["advance"])
    source_rows = glyph["bitmap"]
    assert isinstance(width, int)
    assert isinstance(height, int)
    assert isinstance(x_offset, int)
    assert isinstance(y_offset, int)
    assert isinstance(source_rows, list)

    rows = [0] * BITMAP_ROWS
    top = ascent - (y_offset + height)
    source_bits = ((width + 7) // 8) * 8
    for source_y, encoded in enumerate(source_rows):
        target_y = top + source_y
        if not 0 <= target_y < BITMAP_ROWS:
            continue
        value = int(encoded, 16) if encoded else 0
        for source_x in range(width):
            if value & (1 << (source_bits - 1 - source_x)) == 0:
                continue
            target_x = x_offset + source_x
            if 0 <= target_x < canvas_width:
                rows[target_y] |= 1 << (15 - target_x)

    output = bytearray()
    for bits in rows:
        output.extend(((bits >> 8) & 0xFF, bits & 0xFF))
    return advance, bytes(output)


def array_text(
    name: str,
    characters: list[str],
    rendered: dict[int, tuple[int, bytes]],
) -> str:
    lines = [f"const BitmapGlyph {name}[] PROGMEM = {{"]
    for character in characters:
        advance, bitmap = rendered[ord(character)]
        values = ", ".join(f"0x{value:02X}" for value in bitmap)
        display = (
            character
            if not character.isspace() and character not in {"\\", "'"}
            else f"U+{ord(character):04X}"
        )
        lines.append(
            f"    {{0x{ord(character):04X}, {advance}, {{{values}}}}},  // {display}"
        )
    lines.append("};")
    return "\n".join(lines)


def generate(
    small_bdf_path: Path,
    normal_bdf_path: Path,
    characters_path: Path,
    output_path: Path,
    source_root: Path | None = Path("src"),
) -> bool:
    characters = collect_characters(characters_path, source_root)
    small_ascent, small_glyphs = parse_bdf(small_bdf_path)
    normal_ascent, normal_glyphs = parse_bdf(normal_bdf_path)
    for label, glyphs in (("small", small_glyphs), ("normal", normal_glyphs)):
        missing = [
            character for character in characters if ord(character) not in glyphs
        ]
        if missing:
            raise ValueError(f"{label} BDF is missing: " + "".join(missing))

    small = {
        ord(character): render_bdf_glyph(
            small_glyphs[ord(character)], small_ascent
        )
        for character in characters
    }
    normal = {
        ord(character): render_bdf_glyph(
            normal_glyphs[ord(character)], normal_ascent
        )
        for character in characters
    }

    content = """// Generated by tools/generate_bitmap_font.py.
// Character subset: tools/font_chars.txt plus UTF-8 C/C++ string literals in src/.
// Small font: Fusion Pixel Font 12px proportional zh_hans (OFL-1.1).
// Emphasis font: Fusion Bold Pixel Font 12px proportional zh_hans (OFL-1.1).
#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct BitmapGlyph {
    uint32_t codepoint;
    uint8_t advance;
    uint8_t bitmap[40];
};

"""
    content += array_text("FONT12_GLYPHS", characters, small)
    content += "\n\n"
    content += array_text("FONT_BOLD12_GLYPHS", characters, normal)
    content += "\n\n"
    content += (
        "constexpr size_t FONT12_GLYPH_COUNT = "
        "sizeof(FONT12_GLYPHS) / sizeof(FONT12_GLYPHS[0]);\n"
        "constexpr size_t FONT_BOLD12_GLYPH_COUNT = "
        "sizeof(FONT_BOLD12_GLYPHS) / sizeof(FONT_BOLD12_GLYPHS[0]);\n"
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists() and output_path.read_text(encoding="utf-8") == content:
        print(f"font data already current: {output_path}")
        return False
    output_path.write_text(content, encoding="utf-8", newline="\n")
    print(f"generated {len(characters)} glyphs x 2: {output_path}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--small-bdf", type=Path, required=True)
    parser.add_argument("--normal-bdf", type=Path, required=True)
    parser.add_argument(
        "--characters", type=Path, default=Path("tools/font_chars.txt")
    )
    parser.add_argument(
        "--output", type=Path, default=Path("src/ui/BitmapFontData.h")
    )
    parser.add_argument(
        "--source-root", type=Path, default=Path("src")
    )
    args = parser.parse_args()
    generate(
        args.small_bdf,
        args.normal_bdf,
        args.characters,
        args.output,
        args.source_root,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
