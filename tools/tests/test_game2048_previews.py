import subprocess
import sys
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "captures" / "2048"


class Game2048PreviewTests(unittest.TestCase):
    def test_previews_have_expected_dimensions(self):
        subprocess.run(
            [sys.executable, "tools/render_2048_previews.py"],
            cwd=ROOT,
            check=True,
            capture_output=True,
        )
        for name in ("title.png", "running.png", "merge.png", "target.png",
                     "game_over.png"):
            with Image.open(OUTPUT / name) as image:
                self.assertEqual(image.size, (320, 240))
        with Image.open(OUTPUT / "contact_sheet.png") as image:
            self.assertEqual(image.size, (960, 480))


if __name__ == "__main__":
    unittest.main()
