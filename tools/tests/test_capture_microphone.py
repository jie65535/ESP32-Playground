import importlib.util
import pathlib
import sys
import tempfile
import unittest
from unittest import mock
import wave
import zlib


MODULE_PATH = pathlib.Path(__file__).parents[1] / "capture_microphone.py"
SPEC = importlib.util.spec_from_file_location("capture_microphone", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
CAPTURE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CAPTURE
SPEC.loader.exec_module(CAPTURE)


class _FakeSerial:
    def __init__(self, payload: bytes, chunk_size: int = 11) -> None:
        self._payload = bytearray(payload)
        self._chunk_size = chunk_size

    def read(self, size: int) -> bytes:
        if not self._payload:
            return b""
        count = min(size, self._chunk_size, len(self._payload))
        result = bytes(self._payload[:count])
        del self._payload[:count]
        return result


class _RespondingSerial(_FakeSerial):
    def __init__(self, pcm: bytes, corrupt_crc: bool = False) -> None:
        super().__init__(b"")
        self.pcm = pcm
        self.corrupt_crc = corrupt_crc
        self.commands = []

    def reset_input_buffer(self) -> None:
        self._payload.clear()

    def write(self, data: bytes) -> int:
        command = data.decode("ascii").strip()
        self.commands.append(command)
        if not command.startswith("mic record "):
            return len(data)
        _, _, duration, request_id = command.split()
        request = int(request_id)
        sequence = 7
        header = CAPTURE.FRAME_HEADER_STRUCT.pack(
            CAPTURE.FRAME_MAGIC,
            CAPTURE.FRAME_VERSION,
            CAPTURE.AUDIO_FORMAT_PCM16LE_MONO,
            CAPTURE.FRAME_HEADER_STRUCT.size,
            8000,
            int(duration),
            len(self.pcm),
            request,
            sequence,
        )
        crc = zlib.crc32(self.pcm) & 0xFFFFFFFF
        if self.corrupt_crc:
            crc ^= 0xFFFFFFFF
        trailer = CAPTURE.FRAME_TRAILER_STRUCT.pack(
            CAPTURE.TRAILER_MAGIC, request, sequence, crc
        )
        self._payload.extend(header + self.pcm + trailer)
        return len(data)

    def flush(self) -> None:
        return


def _header(
    request_id: int, sequence: int, payload_size: int = 16000
) -> bytes:
    return CAPTURE.FRAME_HEADER_STRUCT.pack(
        CAPTURE.FRAME_MAGIC,
        CAPTURE.FRAME_VERSION,
        CAPTURE.AUDIO_FORMAT_PCM16LE_MONO,
        CAPTURE.FRAME_HEADER_STRUCT.size,
        8000,
        1000,
        payload_size,
        request_id,
        sequence,
    )


class MicrophoneProtocolTests(unittest.TestCase):
    def test_capture_sends_correlation_id_and_validates_complete_frame(self) -> None:
        device = _RespondingSerial(b"\x01\x00\xff\xff")
        with mock.patch.object(CAPTURE.time, "monotonic_ns", return_value=1234), \
             mock.patch.object(CAPTURE.time, "sleep") as sleep:
            sample_rate, pcm = CAPTURE.capture_microphone(device, 1000, timeout=0.1)
        self.assertEqual(
            device.commands,
            ["mic capture on", "mic record 1000 1234", "mic capture off"],
        )
        sleep.assert_called_once_with(1.1)
        self.assertEqual(sample_rate, 8000)
        self.assertEqual(pcm, b"\x01\x00\xff\xff")

    def test_capture_rejects_crc_mismatch(self) -> None:
        device = _RespondingSerial(b"\x01\x00", corrupt_crc=True)
        with mock.patch.object(CAPTURE.time, "monotonic_ns", return_value=1234), \
             mock.patch.object(CAPTURE.time, "sleep"):
            with self.assertRaisesRegex(ValueError, "CRC mismatch"):
                CAPTURE.capture_microphone(device, 1000, timeout=0.1)
        self.assertEqual(device.commands[-1], "mic capture off")

    def test_header_resynchronizes_after_log_and_stale_frame(self) -> None:
        stale = _header(100, 1, 32) + b"\x12\x34" * 16
        current = _header(200, 2, 16000)
        reader = CAPTURE._BufferedSerialReader(
            _FakeSerial(b"[status] running\r\n" + stale + current)
        )
        self.assertEqual(
            reader.read_header(200, timeout=0.1),
            (8000, 1000, 16000, 200, 2),
        )

    def test_header_rejects_oversized_payload(self) -> None:
        invalid = _header(200, 2, 8000 * 2 * 7)
        reader = CAPTURE._BufferedSerialReader(_FakeSerial(invalid))
        with self.assertRaises(TimeoutError):
            reader.read_header(200, timeout=0.01)

    def test_duration_bounds_are_enforced_before_serial_access(self) -> None:
        with self.assertRaises(ValueError):
            CAPTURE.capture_microphone(object(), 100)
        with self.assertRaises(ValueError):
            CAPTURE.capture_microphone(object(), 6000)


class WavWriterTests(unittest.TestCase):
    def test_writer_creates_mono_pcm16_wav(self) -> None:
        pcm = b"\x00\x00\xff\x7f\x00\x80"
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "microphone.wav"
            CAPTURE.write_wav(path, 8000, pcm)
            with wave.open(str(path), "rb") as recorded:
                self.assertEqual(recorded.getnchannels(), 1)
                self.assertEqual(recorded.getsampwidth(), 2)
                self.assertEqual(recorded.getframerate(), 8000)
                self.assertEqual(recorded.readframes(3), pcm)

    def test_writer_rejects_partial_sample(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "bad.wav"
            with self.assertRaises(ValueError):
                CAPTURE.write_wav(path, 8000, b"\x00")


if __name__ == "__main__":
    unittest.main()
