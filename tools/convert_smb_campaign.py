#!/usr/bin/env python3
"""Compile the 32-level SDL fan project's CSV maps into PGOS C++ data.

The output is self-contained: firmware builds never read the reference tree.
Map rows use three-byte RLE records so the 192 source CSV files occupy only a
small, randomly addressable flash table on the ESP32.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_SOURCE = Path("reference/Super-Mario-Bros/res/data")
DEFAULT_OUTPUT = Path("src/games/PlatformerCampaignData.generated.cpp")
EMPTY_TILE = 0xFFFF
LAYERS = (
    "Background",
    "Underground",
    "Foreground",
    "Above_Foreground",
    "Collectibles",
    "Enemies",
)

LEVEL_TYPE = {
    "NONE": "None",
    "OVERWORLD": "Overworld",
    "UNDERGROUND": "Underground",
    "UNDERWATER": "Underwater",
    "CASTLE": "Castle",
    "START_UNDERGROUND": "StartUnderground",
}
BACKGROUND = {"BLACK": "Black", "BLUE": "Blue"}
DIRECTION = {
    "NONE": "None",
    "UP": "Up",
    "DOWN": "Down",
    "LEFT": "Left",
    "RIGHT": "Right",
}
MOTION = {
    "NONE": "None",
    "ONE_DIRECTION_REPEATED": "OneDirectionRepeated",
    "ONE_DIRECTION_CONTINUOUS": "OneDirectionContinuous",
    "BACK_AND_FORTH": "BackAndForth",
    "GRAVITY": "Gravity",
}
ROTATION = {
    "NONE": "None",
    "CLOCKWISE": "Clockwise",
    "COUNTER_CLOCKWISE": "CounterClockwise",
}
PAIR = r"\((-?\d+)\s*,\s*(-?\d+)\)"


@dataclass(frozen=True)
class EncodedLayer:
    rows: tuple[tuple[int, ...], ...]
    offsets: tuple[int, ...]
    runs: bytes


@dataclass(frozen=True)
class Level:
    world: int
    stage: int
    width: int
    height: int
    layers: tuple[EncodedLayer, ...]
    properties: dict[str, object]


def parse_pair(value: str) -> tuple[int, int]:
    match = re.fullmatch(PAIR, value.strip())
    if not match:
        raise ValueError(f"invalid coordinate: {value!r}")
    return int(match.group(1)), int(match.group(2))


def read_sections(path: Path) -> tuple[dict[str, str], dict[str, str]]:
    scalars: dict[str, str] = {}
    arrays: dict[str, str] = {}
    current: str | None = None
    parts: list[str] = []

    def finish() -> None:
        nonlocal current, parts
        if current is not None:
            arrays[current] = " ".join(parts).replace("\\", " ")
        current = None
        parts = []

    for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        assignment = re.match(r"^([A-Z_]+)\s*=\s*(.*)$", line)
        if assignment:
            finish()
            key, value = assignment.groups()
            if value.startswith("\\") or key in {
                "FLOATING_TEXT",
                "WARP_PIPE",
                "MOVING_PLATFORM",
                "PLATFORM_LEVEL",
                "FIRE_BAR",
                "VINE",
                "TELEPORT_POINT",
            }:
                current = key
                if value != "\\":
                    parts.append(value)
            else:
                scalars[key] = value
        elif current is not None:
            parts.append(line)
        else:
            raise ValueError(f"orphan property line in {path}: {raw_line!r}")
    finish()
    return scalars, arrays


def all_matches(pattern: str, value: str, key: str) -> list[tuple[str, ...]]:
    matches = re.findall(pattern, value)
    residue = re.sub(pattern, "", value)
    residue = residue.replace(",", "").replace("\\", "").strip()
    if residue:
        raise ValueError(f"unparsed {key} data: {residue!r}")
    return [match if isinstance(match, tuple) else (match,) for match in matches]


def parse_properties(path: Path) -> dict[str, object]:
    scalars, arrays = read_sections(path)
    required = {
        "PLAYER_START",
        "CAMERA_START",
        "CAMERA_MAX",
        "LEVEL_TYPE",
        "NEXT_LEVEL",
        "BACKGROUND_COLOR",
    }
    missing = sorted(required - scalars.keys())
    if missing:
        raise ValueError(f"missing properties in {path}: {', '.join(missing)}")

    warps = all_matches(
        PAIR + r"\s*" + PAIR + r"\s*" + PAIR
        + r"\s+(\w+)\s+(\w+)\s+(TRUE|FALSE)\s+(\w+)\s+(\w+)\s*"
        + PAIR,
        arrays.get("WARP_PIPE", ""),
        "WARP_PIPE",
    )
    moving = all_matches(
        PAIR + r"\s+(\w+)\s+(\w+)\s*" + PAIR + r"\s+(TRUE|FALSE)",
        arrays.get("MOVING_PLATFORM", ""),
        "MOVING_PLATFORM",
    )
    pulleys = all_matches(
        PAIR + r"\s*" + PAIR + r"\s+(-?\d+)",
        arrays.get("PLATFORM_LEVEL", ""),
        "PLATFORM_LEVEL",
    )
    fire_bars = all_matches(
        PAIR + r"\s+(-?\d+)\s+(\w+)\s+(\d+)",
        arrays.get("FIRE_BAR", ""),
        "FIRE_BAR",
    )
    vine_source = arrays.get("VINE", "")
    vine_pattern = (
        PAIR + r"\s*" + PAIR + r"\s*" + PAIR + r"\s+(-?\d+)\s*"
        + PAIR + r"\s+(-?\d+)\s+(\w+)\s+(\w+)"
    )
    # World3-1 in the reference repository has a truncated vine record. Its
    # intended trailing values are unambiguous from the level-wide settings.
    truncated_vine = re.fullmatch(
        PAIR + r"\s*" + PAIR + r"\s*" + PAIR + r"\s+(-?\d+)\s*" + PAIR,
        vine_source.strip(),
    )
    if truncated_vine:
        vine_source += (
            f" {scalars['CAMERA_MAX']} {scalars['BACKGROUND_COLOR']} "
            f"{scalars['LEVEL_TYPE']}"
        )
    vines = all_matches(
        vine_pattern,
        vine_source,
        "VINE",
    )
    teleports = all_matches(PAIR, arrays.get("TELEPORT_POINT", ""), "TELEPORT_POINT")
    texts = all_matches(
        PAIR + r"\s*\(([^)]*)\)", arrays.get("FLOATING_TEXT", ""), "FLOATING_TEXT"
    )

    return {
        "player_start": parse_pair(scalars["PLAYER_START"]),
        "camera_start": parse_pair(scalars["CAMERA_START"]),
        "camera_maximum": int(scalars["CAMERA_MAX"]),
        "level_type": LEVEL_TYPE[scalars["LEVEL_TYPE"]],
        "next_level": parse_pair(scalars["NEXT_LEVEL"]),
        "background": BACKGROUND[scalars["BACKGROUND_COLOR"]],
        "warps": warps,
        "moving": moving,
        "pulleys": pulleys,
        "fire_bars": fire_bars,
        "vines": vines,
        "teleports": teleports,
        "texts": texts,
    }


def read_csv(path: Path) -> tuple[tuple[int, ...], ...]:
    with path.open(newline="", encoding="utf-8-sig") as source:
        rows = tuple(tuple(int(cell) for cell in row) for row in csv.reader(source))
    if not rows or not rows[0]:
        raise ValueError(f"empty map layer: {path}")
    width = len(rows[0])
    if any(len(row) != width for row in rows):
        raise ValueError(f"non-rectangular map layer: {path}")
    return rows


def encode_rows(rows: tuple[tuple[int, ...], ...]) -> EncodedLayer:
    offsets = [0]
    encoded = bytearray()
    for row in rows:
        start = 0
        while start < len(row):
            source_value = row[start]
            if source_value < -1 or source_value >= EMPTY_TILE:
                raise ValueError(f"tile ID is outside uint16 range: {source_value}")
            value = EMPTY_TILE if source_value == -1 else source_value
            length = 1
            while (
                start + length < len(row)
                and row[start + length] == row[start]
                and length < 255
            ):
                length += 1
            encoded.extend((length, value & 0xFF, value >> 8))
            start += length
        offsets.append(len(encoded))
    if len(encoded) > 0xFFFF:
        raise ValueError(f"one layer exceeds uint16 RLE offsets: {len(encoded)} bytes")
    return EncodedLayer(rows, tuple(offsets), bytes(encoded))


def decode_rows(layer: EncodedLayer, width: int) -> tuple[tuple[int, ...], ...]:
    rows: list[tuple[int, ...]] = []
    for row_index in range(len(layer.offsets) - 1):
        start, end = layer.offsets[row_index : row_index + 2]
        row: list[int] = []
        for cursor in range(start, end, 3):
            length = layer.runs[cursor]
            value = layer.runs[cursor + 1] | (layer.runs[cursor + 2] << 8)
            row.extend([-1 if value == EMPTY_TILE else value] * length)
        if len(row) != width:
            raise ValueError(f"decoded row width {len(row)} != {width}")
        rows.append(tuple(row))
    return tuple(rows)


def load_campaign(source: Path) -> list[Level]:
    levels: list[Level] = []
    for world in range(1, 9):
        for stage in range(1, 5):
            name = f"World{world}-{stage}"
            directory = source / name
            if not directory.is_dir():
                raise FileNotFoundError(f"missing level directory: {directory}")
            raw_layers = [read_csv(directory / f"{name}_{layer}.csv") for layer in LAYERS]
            height = len(raw_layers[0])
            width = len(raw_layers[0][0])
            if any(len(rows) != height or len(rows[0]) != width for rows in raw_layers):
                raise ValueError(f"layer dimensions differ in {name}")
            encoded = tuple(encode_rows(rows) for rows in raw_layers)
            properties = parse_properties(directory / f"{name}.levelproperties")
            levels.append(Level(world, stage, width, height, encoded, properties))
    return levels


def c_array(values: Iterable[int], indent: str = "    ", per_line: int = 16) -> str:
    items = [str(value) for value in values]
    return "\n".join(
        indent + ", ".join(items[index : index + per_line]) + ","
        for index in range(0, len(items), per_line)
    )


def point(x: str, y: str) -> str:
    return f"{{{int(x)}, {int(y)}}}"


def escaped(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def span(offset: int, count: int) -> str:
    if count > 255:
        raise ValueError(f"per-level property count exceeds uint8: {count}")
    return f"{{{offset}, {count}}}"


def generate(levels: list[Level], source_digest: str) -> str:
    output = [
        '// Generated by tools/convert_smb_campaign.py. Do not edit.',
        f'// Source data SHA-256: {source_digest}',
        '#include "games/PlatformerCampaignData.h"',
        '',
        'namespace pgos {',
        'namespace {',
        '',
    ]
    for level in levels:
        stem = f"LEVEL_{level.world}_{level.stage}"
        for layer_name, layer in zip(LAYERS, level.layers):
            symbol = f"{stem}_{layer_name.upper()}"
            output.extend(
                [
                    f"const uint16_t {symbol}_ROWS[] = {{",
                    c_array(layer.offsets),
                    "};",
                    f"const uint8_t {symbol}_RUNS[] = {{",
                    c_array(layer.runs),
                    "};",
                    "",
                ]
            )
    output.extend(['}  // namespace', ''])

    properties_by_name = {
        "WARPS": "warps",
        "MOVING_PLATFORMS": "moving",
        "PULLEYS": "pulleys",
        "FIRE_BARS": "fire_bars",
        "VINES": "vines",
        "TELEPORTS": "teleports",
        "FLOATING_TEXTS": "texts",
    }
    flattened: dict[str, list[tuple[str, ...]]] = {}
    spans: dict[tuple[int, int, str], tuple[int, int]] = {}
    for cpp_name, property_name in properties_by_name.items():
        records: list[tuple[str, ...]] = []
        for level in levels:
            values = level.properties[property_name]
            assert isinstance(values, list)
            spans[(level.world, level.stage, property_name)] = (len(records), len(values))
            records.extend(values)
        flattened[cpp_name] = records

    output.append('const PlatformerWarpData PLATFORMER_CAMPAIGN_WARPS[] = {')
    for value in flattened["WARPS"]:
        output.append(
            "    {"
            + ", ".join(
                (
                    point(value[0], value[1]),
                    point(value[2], value[3]),
                    point(value[4], value[5]),
                    f"PlatformerDirection::{DIRECTION[value[6]]}",
                    f"PlatformerDirection::{DIRECTION[value[7]]}",
                    "true" if value[8] == "TRUE" else "false",
                    f"PlatformerBackgroundColor::{BACKGROUND[value[9]]}",
                    f"PlatformerLevelType::{LEVEL_TYPE[value[10]]}",
                    str(int(value[11])),
                    str(int(value[12])),
                )
            )
            + "},"
        )
    output.extend(["};", ""])

    output.append(
        'const PlatformerMovingPlatformData PLATFORMER_CAMPAIGN_MOVING_PLATFORMS[] = {'
    )
    for value in flattened["MOVING_PLATFORMS"]:
        output.append(
            "    {"
            + ", ".join(
                (
                    point(value[0], value[1]),
                    f"PlatformerMotionType::{MOTION[value[2]]}",
                    f"PlatformerDirection::{DIRECTION[value[3]]}",
                    str(int(value[4])),
                    str(int(value[5])),
                    "true" if value[6] == "TRUE" else "false",
                )
            )
            + "},"
        )
    output.extend(["};", ""])

    output.append('const PlatformerPulleyData PLATFORMER_CAMPAIGN_PULLEYS[] = {')
    for value in flattened["PULLEYS"]:
        output.append(
            f"    {{{point(value[0], value[1])}, {point(value[2], value[3])}, {int(value[4])}}},"
        )
    output.extend(["};", ""])

    output.append('const PlatformerFireBarData PLATFORMER_CAMPAIGN_FIRE_BARS[] = {')
    for value in flattened["FIRE_BARS"]:
        output.append(
            f"    {{{point(value[0], value[1])}, {int(value[2])}, "
            f"PlatformerRotationDirection::{ROTATION[value[3]]}, {int(value[4])}}},"
        )
    output.extend(["};", ""])

    output.append('const PlatformerVineData PLATFORMER_CAMPAIGN_VINES[] = {')
    for value in flattened["VINES"]:
        output.append(
            "    {"
            + ", ".join(
                (
                    point(value[0], value[1]),
                    point(value[2], value[3]),
                    point(value[4], value[5]),
                    str(int(value[6])),
                    point(value[7], value[8]),
                    str(int(value[9])),
                    f"PlatformerBackgroundColor::{BACKGROUND[value[10]]}",
                    f"PlatformerLevelType::{LEVEL_TYPE[value[11]]}",
                )
            )
            + "},"
        )
    output.extend(["};", ""])

    output.append('const PlatformerTeleportData PLATFORMER_CAMPAIGN_TELEPORTS[] = {')
    for value in flattened["TELEPORTS"]:
        output.append(f"    {{{int(value[0])}, {int(value[1])}}},")
    output.extend(["};", ""])

    output.append('const PlatformerFloatingTextData PLATFORMER_CAMPAIGN_FLOATING_TEXTS[] = {')
    for value in flattened["FLOATING_TEXTS"]:
        output.append(
            f'    {{{point(value[0], value[1])}, "{escaped(value[2].strip())}"}},'
        )
    output.extend(["};", ""])

    output.append(
        'const PlatformerCampaignLevel '
        'PLATFORMER_CAMPAIGN_LEVELS[PLATFORMER_CAMPAIGN_LEVEL_COUNT] = {'
    )
    for level in levels:
        props = level.properties
        stem = f"LEVEL_{level.world}_{level.stage}"
        output.extend(
            [
                "    {",
                f"        {level.world}, {level.stage}, {level.width}, {level.height},",
                f"        {point(*props['player_start'])}, {point(*props['camera_start'])}, "
                f"{props['camera_maximum']},",
                f"        {props['next_level'][0]}, {props['next_level'][1]},",
                f"        PlatformerLevelType::{props['level_type']},",
                f"        PlatformerBackgroundColor::{props['background']},",
                "        {",
            ]
        )
        for layer_name, layer in zip(LAYERS, level.layers):
            symbol = f"{stem}_{layer_name.upper()}"
            output.append(
                f"            {{{symbol}_ROWS, {symbol}_RUNS, {len(layer.runs)}}},"
            )
        output.append("        },")
        for property_name in properties_by_name.values():
            offset, count = spans[(level.world, level.stage, property_name)]
            output.append(f"        {span(offset, count)},")
        output.append("    },")
    output.extend(["};", "", "}  // namespace pgos", ""])
    return "\n".join(output)


def campaign_digest(source: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(source.glob("World*-*/*")):
        if path.suffix not in {".csv", ".levelproperties"}:
            continue
        digest.update(path.relative_to(source).as_posix().encode("utf-8"))
        digest.update(path.read_bytes())
    return digest.hexdigest()


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
    parser.add_argument("--verify", action="store_true", help="decode and compare every CSV cell")
    args = parser.parse_args()

    levels = load_campaign(args.source)
    if args.verify:
        for level in levels:
            for layer in level.layers:
                if decode_rows(layer, level.width) != layer.rows:
                    raise ValueError(f"RLE round-trip failed for {level.world}-{level.stage}")
    content = generate(levels, campaign_digest(args.source))
    changed = write_if_changed(args.output, content)
    packed_bytes = sum(len(layer.runs) + len(layer.offsets) * 2 for level in levels for layer in level.layers)
    print(
        f"{'generated' if changed else 'unchanged'} {len(levels)} levels, "
        f"{packed_bytes} packed map bytes -> {args.output}"
    )


if __name__ == "__main__":
    main()
