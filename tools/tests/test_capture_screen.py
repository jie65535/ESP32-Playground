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


class _FakeSerial:
    def __init__(self, payload: bytes, chunk_size: int = 17) -> None:
        self._payload = bytearray(payload)
        self._chunk_size = chunk_size

    def read(self, size: int) -> bytes:
        if not self._payload:
            return b""
        count = min(size, self._chunk_size, len(self._payload))
        result = bytes(self._payload[:count])
        del self._payload[:count]
        return result


def _frame_header(request_id: int, sequence: int) -> bytes:
    return CAPTURE.FRAME_HEADER_STRUCT.pack(
        CAPTURE.FRAME_MAGIC,
        CAPTURE.FRAME_VERSION,
        CAPTURE.PIXEL_FORMAT_RGB565BE,
        CAPTURE.FRAME_HEADER_STRUCT.size,
        320,
        240,
        153600,
        request_id,
        sequence,
    )


class ScreenshotProtocolTests(unittest.TestCase):
    def test_header_resynchronizes_after_binary_stale_frame(self) -> None:
        stale_frame = _frame_header(100, 1) + b"\x08\x83" * 23
        current_frame = _frame_header(200, 2)
        reader = CAPTURE._BufferedSerialReader(
            _FakeSerial(stale_frame + current_frame)
        )
        self.assertEqual(
            reader.read_header(200, timeout=0.1),
            (153600, 200, 2),
        )

    def test_capture_default_timeout_covers_full_usb_frame(self) -> None:
        self.assertGreaterEqual(
            CAPTURE.capture_framebuffer.__defaults__[0], 30.0
        )

    def test_trailer_validates_request_sequence_and_crc(self) -> None:
        trailer = CAPTURE.FRAME_TRAILER_STRUCT.pack(
            CAPTURE.TRAILER_MAGIC, 200, 2, 0xCBF43926
        )
        self.assertEqual(
            CAPTURE._BufferedSerialReader(_FakeSerial(trailer)).read_trailer(
                200, 2, timeout=0.1
            ),
            0xCBF43926,
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
