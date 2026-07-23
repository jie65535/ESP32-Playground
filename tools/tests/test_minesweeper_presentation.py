import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
APP_SOURCE = ROOT / "src" / "apps" / "MinesweeperApp.cpp"


class MinesweeperPresentationTests(unittest.TestCase):
    def test_lvgl_small_font_strings_use_ascii_punctuation(self):
        source = APP_SOURCE.read_text(encoding="utf-8")
        self.assertNotIn("×", source)
        self.assertNotIn("·", source)

    def test_native_layout_and_timing_suite(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "minesweeper_presentation_test.exe"
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-O2",
                    "-Isrc",
                    "tools/tests/minesweeper_presentation_test.cpp",
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
