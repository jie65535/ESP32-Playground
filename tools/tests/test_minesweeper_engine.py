import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class MinesweeperEngineTests(unittest.TestCase):
    def test_native_rule_and_generation_suite(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "minesweeper_engine_test.exe"
            command = [
                compiler,
                "-std=c++17",
                "-O2",
                "-DPGOS_MINESWEEPER_TESTING=1",
                "-Isrc",
                "src/games/MinesweeperEngine.cpp",
                "tools/tests/minesweeper_engine_test.cpp",
                "-o",
                str(executable),
            ]
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True)
            result = subprocess.run(
                [str(executable)],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertIn("beginner:", result.stdout)
            self.assertIn("intermediate:", result.stdout)
            self.assertIn("expert:", result.stdout)


if __name__ == "__main__":
    unittest.main()
