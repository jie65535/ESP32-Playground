import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class PlatformerLevelRuntimeTests(unittest.TestCase):
    def test_native_level_runtime(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "platformer_level_runtime_test.exe"
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
                    "tools/tests/platformer_level_runtime_test.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
            )
            subprocess.run([str(executable)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
