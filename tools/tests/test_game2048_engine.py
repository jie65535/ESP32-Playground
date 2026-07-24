import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class Game2048EngineTests(unittest.TestCase):
    def test_native_rule_suite(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "game2048_engine_test.exe"
            command = [
                compiler,
                "-std=c++17",
                "-DPGOS_2048_TESTING=1",
                "-Isrc",
                "src/games/Game2048Engine.cpp",
                "tools/tests/game2048_engine_test.cpp",
                "-o",
                str(executable),
            ]
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True)
            subprocess.run([str(executable)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
