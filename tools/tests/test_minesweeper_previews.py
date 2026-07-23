import importlib.util
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "render_minesweeper_previews.py"


class MinesweeperPreviewTests(unittest.TestCase):
    def test_preview_frames_are_full_screen_and_rgb(self):
        spec = importlib.util.spec_from_file_location("minesweeper_previews", SCRIPT)
        assert spec is not None and spec.loader is not None
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        frames = (
            module.render_title(),
            module.render_game("MINESWEEPER", 9, 9, 18),
            module.render_game("MINESWEEPER", 30, 16, 10),
            module.render_custom(),
            module.render_records(),
            module.render_game("MINESWEEPER", 16, 16, 11, "paused"),
            module.render_game("MINESWEEPER", 16, 16, 11, "generating"),
            module.render_game("MINESWEEPER", 16, 16, 11, "won"),
            module.render_game("MINESWEEPER", 16, 16, 11, "lost"),
        )
        for frame in frames:
            self.assertEqual(frame.size, (320, 240))
            self.assertEqual(frame.mode, "RGB")
            self.assertNotEqual(frame.getbbox(), None)


if __name__ == "__main__":
    unittest.main()
