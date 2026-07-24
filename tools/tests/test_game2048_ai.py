import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class Game2048AiTests(unittest.TestCase):
    def test_native_expectimax_suite(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "game2048_ai_test.exe"
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-DPGOS_2048_TESTING=1",
                    "-Isrc",
                    "src/games/Game2048Ai.cpp",
                    "src/games/Game2048Engine.cpp",
                    "tools/tests/game2048_ai_test.cpp",
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
