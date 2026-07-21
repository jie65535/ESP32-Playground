import importlib.util
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "capture_platformer_map.py"


def load_capture_tool():
    spec = importlib.util.spec_from_file_location("platformer_map_capture", SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError("unable to load map capture tool")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PlatformerMapCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.capture_tool = load_capture_tool()

    def test_frame_count_matches_world_width(self):
        tool = self.capture_tool
        self.assertEqual(tool.FRAME_COUNT, 13)
        self.assertEqual(
            tool.FRAME_WIDTH + (tool.FRAME_COUNT - 1) * tool.STEP_WIDTH,
            tool.WORLD_WIDTH,
        )

    def test_stitch_segments_uses_overlap_once(self):
        tool = self.capture_tool
        with tempfile.TemporaryDirectory() as directory:
            segment_dir = Path(directory)
            for index in range(tool.FRAME_COUNT):
                image = Image.new("RGB", (tool.FRAME_WIDTH, tool.FRAME_HEIGHT))
                image.paste((index, 0, 0), (0, tool.WORLD_TOP,
                                            tool.FRAME_WIDTH, tool.FRAME_HEIGHT))
                image.save(segment_dir / f"map-{index:02d}.png")

            panorama = tool.stitch_segments(segment_dir)
            self.assertEqual(panorama.size, (tool.WORLD_WIDTH, tool.WORLD_HEIGHT))
            self.assertEqual(panorama.getpixel((0, 0)), (0, 0, 0))
            self.assertEqual(panorama.getpixel((tool.STEP_WIDTH, 0)), (0, 0, 0))
            self.assertEqual(panorama.getpixel((tool.FRAME_WIDTH, 0)), (1, 0, 0))
            self.assertEqual(panorama.getpixel((tool.WORLD_WIDTH - 1, 0)),
                             (12, 0, 0))


if __name__ == "__main__":
    unittest.main()
