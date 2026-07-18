import importlib.util
import pathlib
import struct
import sys
import tempfile
import unittest


MODULE_PATH = pathlib.Path(__file__).parents[1] / "capture_screen.py"
SPEC = importlib.util.spec_from_file_location("capture_screen", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
CAPTURE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CAPTURE
SPEC.loader.exec_module(CAPTURE)


class PixelConversionTests(unittest.TestCase):
    def test_rgb565_primary_colors_expand_to_rgb888(self) -> None:
        payload = bytes.fromhex("F800 07E0 001F FFFF")
        rows = CAPTURE.rgb565be_to_rgb888(payload, 4, 1)
        self.assertEqual(
            rows,
            bytes(
                [
                    0,
                    255,
                    0,
                    0,
                    0,
                    255,
                    0,
                    0,
                    0,
                    255,
                    255,
                    255,
                    255,
                ]
            ),
        )

    def test_wrong_payload_size_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            CAPTURE.rgb565be_to_rgb888(b"\x00\x00", 2, 1)

    def test_dib_is_bottom_up_bgr_with_padded_rows(self) -> None:
        payload = bytes.fromhex("F800 07E0 001F FFFF")
        dib = CAPTURE.rgb565be_to_dib(payload, 2, 2)
        self.assertEqual(struct.unpack("<IiiHH", dib[:16]), (40, 2, 2, 1, 24))
        self.assertEqual(
            dib[40:],
            bytes(
                [
                    255,
                    0,
                    0,
                    255,
                    255,
                    255,
                    0,
                    0,
                    0,
                    0,
                    255,
                    0,
                    255,
                    0,
                    0,
                    0,
                ]
            ),
        )


class PngWriterTests(unittest.TestCase):
    def test_writer_creates_rgb_png_with_expected_dimensions(self) -> None:
        rows = CAPTURE.rgb565be_to_rgb888(bytes.fromhex("F800 07E0"), 2, 1)
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "screen.png"
            CAPTURE.write_png(path, 2, 1, rows)
            data = path.read_bytes()
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack(">II", data[16:24]), (2, 1))


if __name__ == "__main__":
    unittest.main()
