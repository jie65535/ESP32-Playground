import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import convert_smb_campaign as campaign  # noqa: E402


class PlatformerCampaignDataTests(unittest.TestCase):
    def test_native_campaign_decoder(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "platformer_campaign_data_test.exe"
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-Isrc",
                    "src/games/PlatformerCampaignData.cpp",
                    "src/games/PlatformerCampaignData.generated.cpp",
                    "src/games/PlatformerTileAssets.cpp",
                    "src/games/PlatformerTileAssets.generated.cpp",
                    "tools/tests/platformer_campaign_data_test.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
            )
            subprocess.run([str(executable)], cwd=ROOT, check=True)

    def test_reference_round_trip_and_generated_output(self):
        source = campaign.DEFAULT_SOURCE
        if not source.is_dir():
            self.skipTest("local reference campaign is unavailable")

        levels = campaign.load_campaign(source)
        self.assertEqual(len(levels), 32)
        self.assertEqual(
            [(level.world, level.stage) for level in levels],
            [(world, stage) for world in range(1, 9) for stage in range(1, 5)],
        )

        for index, level in enumerate(levels):
            for layer in level.layers:
                self.assertEqual(campaign.decode_rows(layer, level.width), layer.rows)
            expected_next = (
                (levels[index + 1].world, levels[index + 1].stage)
                if index + 1 < len(levels)
                else (0, 0)
            )
            self.assertEqual(level.properties["next_level"], expected_next)

        packed_size = sum(
            len(layer.runs) + len(layer.offsets) * 2
            for level in levels
            for layer in level.layers
        )
        self.assertEqual(packed_size, 86883)
        self.assertEqual(len(levels[8].properties["vines"]), 1)  # repaired World3-1
        self.assertEqual(
            levels[-1].properties["teleports"],
            [("100", "32"), ("165", "100"), ("244", "181")],
        )

        generated = campaign.generate(levels, campaign.campaign_digest(source))
        committed = (
            ROOT / "src/games/PlatformerCampaignData.generated.cpp"
        ).read_text(encoding="utf-8")
        self.assertEqual(generated, committed)


if __name__ == "__main__":
    unittest.main()
