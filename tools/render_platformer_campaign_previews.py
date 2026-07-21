#!/usr/bin/env python3
"""Build the native tile renderer and make a 32-level first-screen sheet."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import convert_smb_campaign as campaign  # noqa: E402


def build_preview(executable: Path) -> None:
    compiler = shutil.which("g++")
    if compiler is None:
        raise RuntimeError("g++ is unavailable")
    subprocess.run(
        [
            compiler,
            "-std=c++17",
            "-O2",
            "-Isrc",
            "src/games/PlatformerCampaignData.cpp",
            "src/games/PlatformerCampaignData.generated.cpp",
            "src/games/PlatformerTileAssets.cpp",
            "src/games/PlatformerTileAssets.generated.cpp",
            "src/games/PlatformerLevelRuntime.cpp",
            "src/games/PlatformerTileRenderer.cpp",
            "tools/platformer_campaign_preview.cpp",
            "-o",
            str(executable),
        ],
        cwd=ROOT,
        check=True,
    )


def render(output: Path) -> None:
    levels = campaign.load_campaign(campaign.DEFAULT_SOURCE)
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        executable = temporary / "platformer_campaign_preview.exe"
        build_preview(executable)
        cells: list[Image.Image] = []
        for level in levels:
            camera_x, camera_y = level.properties["camera_start"]
            ppm = temporary / f"{level.world}-{level.stage}.ppm"
            subprocess.run(
                [
                    str(executable),
                    str(level.world),
                    str(level.stage),
                    str(camera_x),
                    str(camera_y),
                    str(ppm),
                ],
                cwd=ROOT,
                check=True,
            )
            with Image.open(ppm) as source:
                cell = source.convert("RGB")
            draw = ImageDraw.Draw(cell)
            draw.rectangle((2, 2, 39, 16), fill=(0, 0, 0))
            draw.text((5, 4), f"{level.world}-{level.stage}", fill=(255, 255, 255))
            cells.append(cell)

        sheet = Image.new("RGB", (320 * 4, 218 * 8))
        for index, cell in enumerate(cells):
            sheet.paste(cell, ((index % 4) * 320, (index // 4) * 218))
        output.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(output)
        print(f"Saved {sheet.width}x{sheet.height} campaign preview: {output}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("captures/platformer/campaign-first-screens.png"),
    )
    args = parser.parse_args()
    render(args.output)


if __name__ == "__main__":
    main()
