import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import convert_smb_tile_assets as tile_assets  # noqa: E402


class PlatformerTileAssetTests(unittest.TestCase):
    def test_native_tile_renderer(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "platformer_tile_renderer_test.exe"
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
                    "tools/tests/platformer_tile_renderer_test.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
            )
            subprocess.run([str(executable)], cwd=ROOT, check=True)

    def test_reference_assets_round_trip_and_generated_output(self):
        source = tile_assets.DEFAULT_SOURCE
        if not source.is_dir():
            self.skipTest("local reference sprite sheets are unavailable")
        sheets = [
            tile_assets.encode_sheet(source, spec) for spec in tile_assets.SHEETS
        ]
        self.assertEqual(
            [
                (
                    sheet.spec.name,
                    len(sheet.palette),
                    sheet.bits_per_pixel,
                    len(sheet.pixels),
                )
                for sheet in sheets
            ],
            [
                ("BLOCK", 26, 5, 168960),
                ("ENEMY", 15, 4, 67200),
                ("PLAYER", 18, 5, 64000),
            ],
        )
        self.assertEqual(sum(len(sheet.pixels) for sheet in sheets), 300160)
        self.assertEqual(
            tile_assets.block_references(source)[592:597],
            [337, 101, 150, 151, 339],
        )
        generated = tile_assets.generate(source)
        committed = (
            ROOT / "src/games/PlatformerTileAssets.generated.cpp"
        ).read_text(encoding="utf-8")
        self.assertEqual(generated, committed)


if __name__ == "__main__":
    unittest.main()
