#!/usr/bin/env python3
"""Capture the Playground microphone ring buffer over USB CDC as WAV."""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys
import time
import wave
import zlib
from datetime import datetime


FRAME_MAGIC = b"PGM1"
TRAILER_MAGIC = b"PGM2"
FRAME_VERSION = 1
AUDIO_FORMAT_PCM16LE_MONO = 1
FRAME_HEADER_STRUCT = struct.Struct("<4sBBHIIIII")
FRAME_TRAILER_STRUCT = struct.Struct("<4sIII")
MIN_DURATION_MS = 250
MAX_DURATION_MS = 5000
CAPTURE_WARMUP_SECONDS = 0.1


def open_serial(port: str, baud: int):
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError(
            "pyserial is required; run: python -m pip install -r tools/requirements.txt"
        ) from exc

    device = serial.Serial()
    device.port = port
    device.baudrate = baud
    device.timeout = 0.2
    device.dtr = False
    device.rts = False
    device.open()
    return device


def send_command(device, command: str) -> None:
    device.write((command.strip() + "\n").encode("ascii"))
    device.flush()


class _BufferedSerialReader:
    """Raw byte reader that can recover after logs or an abandoned frame."""

    def __init__(self, device) -> None:
        self.device = device
        self.buffer = bytearray()

    def _fill(self, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        while True:
            chunk = self.device.read(4096)
            if chunk:
                self.buffer.extend(chunk)
                return
            if time.monotonic() >= deadline:
                raise TimeoutError("microphone frame data not received")

    def read_exact(self, size: int, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while len(self.buffer) < size:
            chunk = self.device.read(4096)
            if chunk:
                self.buffer.extend(chunk)
                continue
            if time.monotonic() >= deadline:
                raise TimeoutError(
                    f"received {len(self.buffer)} of {size} microphone bytes"
                )
        result = bytes(self.buffer[:size])
        del self.buffer[:size]
        return result

    def read_header(
        self, request_id: int, timeout: float
    ) -> tuple[int, int, int, int, int]:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            magic_index = self.buffer.find(FRAME_MAGIC)
            if magic_index < 0:
                if len(self.buffer) > len(FRAME_MAGIC) - 1:
                    del self.buffer[: -(len(FRAME_MAGIC) - 1)]
                self._fill(max(0.0, deadline - time.monotonic()))
                continue
            if magic_index > 0:
                del self.buffer[:magic_index]
            if len(self.buffer) < FRAME_HEADER_STRUCT.size:
                self._fill(max(0.0, deadline - time.monotonic()))
                continue

            fields = FRAME_HEADER_STRUCT.unpack(
                bytes(self.buffer[: FRAME_HEADER_STRUCT.size])
            )
            (
                magic,
                version,
                audio_format,
                header_bytes,
                sample_rate,
                requested_ms,
                payload_size,
                frame_request_id,
                sequence,
            ) = fields
            valid = (
                magic == FRAME_MAGIC
                and version == FRAME_VERSION
                and audio_format == AUDIO_FORMAT_PCM16LE_MONO
                and header_bytes == FRAME_HEADER_STRUCT.size
                and 1000 <= sample_rate <= 192000
                and MIN_DURATION_MS <= requested_ms <= MAX_DURATION_MS
                and payload_size % 2 == 0
                and payload_size <= sample_rate * 2 * 6
            )
            if not valid:
                del self.buffer[:1]
                continue
            del self.buffer[: FRAME_HEADER_STRUCT.size]
            if frame_request_id != request_id:
                continue
            return (
                sample_rate,
                requested_ms,
                payload_size,
                frame_request_id,
                sequence,
            )
        raise TimeoutError("microphone frame header not received")

    def read_trailer(self, request_id: int, sequence: int, timeout: float) -> int:
        raw = self.read_exact(FRAME_TRAILER_STRUCT.size, timeout)
        magic, trailer_request_id, trailer_sequence, payload_crc = (
            FRAME_TRAILER_STRUCT.unpack(raw)
        )
        if (
            magic != TRAILER_MAGIC
            or trailer_request_id != request_id
            or trailer_sequence != sequence
        ):
            raise ValueError(f"invalid microphone trailer: {raw!r}")
        return payload_crc


def capture_microphone(
    device, duration_ms: int, timeout: float = 15.0
) -> tuple[int, bytes]:
    if not MIN_DURATION_MS <= duration_ms <= MAX_DURATION_MS:
        raise ValueError(
            f"duration must be between {MIN_DURATION_MS} and {MAX_DURATION_MS} ms"
        )

    request_id = int(time.monotonic_ns() & 0x7FFFFFFF) or 1
    capture_started = False
    device.reset_input_buffer()
    send_command(device, "mic capture on")
    capture_started = True
    try:
        time.sleep(duration_ms / 1000.0 + CAPTURE_WARMUP_SECONDS)
        device.reset_input_buffer()
        send_command(device, f"mic record {duration_ms} {request_id}")
        reader = _BufferedSerialReader(device)
        sample_rate, _, payload_size, frame_request_id, sequence = (
            reader.read_header(request_id, timeout)
        )
        payload = reader.read_exact(payload_size, timeout)
        received_crc = reader.read_trailer(frame_request_id, sequence, timeout)
        actual_crc = zlib.crc32(payload) & 0xFFFFFFFF
        if received_crc != actual_crc:
            raise ValueError(
                f"microphone CRC mismatch: received {received_crc:08X}, "
                f"calculated {actual_crc:08X}"
            )
        if not payload:
            raise ValueError("device returned no microphone samples")
        return sample_rate, payload
    finally:
        if capture_started:
            send_command(device, "mic capture off")


def write_wav(path: pathlib.Path, sample_rate: int, pcm16le: bytes) -> None:
    if len(pcm16le) % 2 != 0:
        raise ValueError("PCM16 payload length must be even")
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(pcm16le)


def default_output_path() -> pathlib.Path:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return pathlib.Path("captures") / f"microphone-{timestamp}.wav"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture the Playground microphone as mono PCM16 WAV"
    )
    parser.add_argument("--port", required=True, help="USB CDC port, for example COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=int, default=3000, help="250 to 5000 ms")
    parser.add_argument("--output", type=pathlib.Path, default=None)
    parser.add_argument("--timeout", type=float, default=15.0)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    output = args.output or default_output_path()
    try:
        device = open_serial(args.port, args.baud)
    except (RuntimeError, OSError) as exc:
        print(exc, file=sys.stderr)
        return 2

    try:
        sample_rate, payload = capture_microphone(
            device, args.duration, args.timeout
        )
        write_wav(output, sample_rate, payload)
    except (OSError, TimeoutError, ValueError) as exc:
        print(f"capture failed: {exc}", file=sys.stderr)
        return 1
    finally:
        device.close()

    actual_ms = len(payload) * 1000 // (sample_rate * 2)
    print(
        f"Saved {actual_ms} ms mono PCM16LE at {sample_rate} Hz: "
        f"{output.resolve()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
