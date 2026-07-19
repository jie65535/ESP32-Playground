#!/usr/bin/env python3
"""Capture Playground's PSRAM framebuffer over USB CDC and save a PNG."""

from __future__ import annotations

import argparse
import binascii
import os
import pathlib
import struct
import sys
import time
import zlib
from datetime import datetime


FRAME_MAGIC = b"PGS2"
TRAILER_MAGIC = b"PGE2"
FRAME_VERSION = 2
PIXEL_FORMAT_RGB565BE = 1
SUPPORTED_FORMAT = "RGB565BE"
FRAME_HEADER_STRUCT = struct.Struct("<4sBBHHHIII")
FRAME_TRAILER_STRUCT = struct.Struct("<4sIII")


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    checksum = binascii.crc32(kind)
    checksum = binascii.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def rgb565be_to_rgb888(payload: bytes, width: int, height: int) -> bytes:
    expected = width * height * 2
    if len(payload) != expected:
        raise ValueError(f"RGB565 payload is {len(payload)} bytes, expected {expected}")

    rows = bytearray()
    source = memoryview(payload)
    offset = 0
    for _ in range(height):
        rows.append(0)  # PNG filter type 0
        for _ in range(width):
            value = (source[offset] << 8) | source[offset + 1]
            offset += 2
            red5 = (value >> 11) & 0x1F
            green6 = (value >> 5) & 0x3F
            blue5 = value & 0x1F
            rows.extend(
                (
                    (red5 << 3) | (red5 >> 2),
                    (green6 << 2) | (green6 >> 4),
                    (blue5 << 3) | (blue5 >> 2),
                )
            )
    return bytes(rows)


def rgb565be_to_dib(payload: bytes, width: int, height: int) -> bytes:
    """Convert RGB565BE to a bottom-up 24-bit Windows CF_DIB payload."""
    expected = width * height * 2
    if len(payload) != expected:
        raise ValueError(f"RGB565 payload is {len(payload)} bytes, expected {expected}")

    stride = (width * 3 + 3) & ~3
    image_size = stride * height
    header = struct.pack(
        "<IiiHHIIiiII",
        40,
        width,
        height,
        1,
        24,
        0,
        image_size,
        2835,
        2835,
        0,
        0,
    )
    pixels = bytearray()
    source = memoryview(payload)
    padding = bytes(stride - width * 3)
    for y in range(height - 1, -1, -1):
        offset = y * width * 2
        for _ in range(width):
            value = (source[offset] << 8) | source[offset + 1]
            offset += 2
            red5 = (value >> 11) & 0x1F
            green6 = (value >> 5) & 0x3F
            blue5 = value & 0x1F
            pixels.extend(
                (
                    (blue5 << 3) | (blue5 >> 2),
                    (green6 << 2) | (green6 >> 4),
                    (red5 << 3) | (red5 >> 2),
                )
            )
        pixels.extend(padding)
    return header + pixels


def copy_dib_to_clipboard(dib: bytes) -> None:
    if os.name != "nt":
        raise RuntimeError("image clipboard copy is currently supported on Windows only")

    import ctypes
    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    user32.OpenClipboard.argtypes = [wintypes.HWND]
    user32.OpenClipboard.restype = wintypes.BOOL
    user32.EmptyClipboard.restype = wintypes.BOOL
    user32.SetClipboardData.argtypes = [wintypes.UINT, wintypes.HANDLE]
    user32.SetClipboardData.restype = wintypes.HANDLE
    user32.CloseClipboard.restype = wintypes.BOOL
    kernel32.GlobalAlloc.argtypes = [wintypes.UINT, ctypes.c_size_t]
    kernel32.GlobalAlloc.restype = wintypes.HGLOBAL
    kernel32.GlobalLock.argtypes = [wintypes.HGLOBAL]
    kernel32.GlobalLock.restype = ctypes.c_void_p
    kernel32.GlobalUnlock.argtypes = [wintypes.HGLOBAL]
    kernel32.GlobalFree.argtypes = [wintypes.HGLOBAL]

    clipboard_open = False
    for _ in range(20):
        if user32.OpenClipboard(None):
            clipboard_open = True
            break
        time.sleep(0.01)
    if not clipboard_open:
        raise OSError(ctypes.get_last_error(), "OpenClipboard failed")

    handle = None
    transferred = False
    try:
        if not user32.EmptyClipboard():
            raise OSError(ctypes.get_last_error(), "EmptyClipboard failed")
        handle = kernel32.GlobalAlloc(0x0002, len(dib))  # GMEM_MOVEABLE
        if not handle:
            raise MemoryError("GlobalAlloc failed")
        target = kernel32.GlobalLock(handle)
        if not target:
            raise OSError(ctypes.get_last_error(), "GlobalLock failed")
        try:
            ctypes.memmove(target, dib, len(dib))
        finally:
            kernel32.GlobalUnlock(handle)
        if not user32.SetClipboardData(8, handle):  # CF_DIB
            raise OSError(ctypes.get_last_error(), "SetClipboardData failed")
        transferred = True
    finally:
        user32.CloseClipboard()
        if handle and not transferred:
            kernel32.GlobalFree(handle)


def write_png(path: pathlib.Path, width: int, height: int, rgb_rows: bytes) -> None:
    signature = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = bytearray(signature)
    png.extend(_png_chunk(b"IHDR", ihdr))
    png.extend(_png_chunk(b"IDAT", zlib.compress(rgb_rows, level=9)))
    png.extend(_png_chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


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


def read_exact(device, size: int, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    data = bytearray()
    while len(data) < size:
        chunk = device.read(size - len(data))
        if chunk:
            data.extend(chunk)
            continue
        if time.monotonic() >= deadline:
            raise TimeoutError(f"received {len(data)} of {size} screenshot bytes")
    return bytes(data)


class _BufferedSerialReader:
    """Byte-stream reader that can resynchronize after an abandoned frame."""

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
                raise TimeoutError("screenshot frame data not received")

    def read_exact(self, size: int, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while len(self.buffer) < size:
            chunk = self.device.read(4096)
            if chunk:
                self.buffer.extend(chunk)
                continue
            if time.monotonic() >= deadline:
                raise TimeoutError(
                    f"received {len(self.buffer)} of {size} screenshot bytes"
                )
        result = bytes(self.buffer[:size])
        del self.buffer[:size]
        return result

    def read_header(self, request_id: int, timeout: float) -> tuple[int, int, int]:
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
                pixel_format,
                header_bytes,
                width,
                height,
                payload_size,
                frame_request_id,
                sequence,
            ) = fields
            if (
                magic != FRAME_MAGIC
                or version != FRAME_VERSION
                or pixel_format != PIXEL_FORMAT_RGB565BE
                or header_bytes != FRAME_HEADER_STRUCT.size
                or width != 320
                or height != 240
                or payload_size != width * height * 2
            ):
                del self.buffer[:1]
                continue
            del self.buffer[: FRAME_HEADER_STRUCT.size]
            if frame_request_id != request_id:
                # A stale frame from an earlier timed-out capture.  Its
                # payload remains in the stream; scan through it until the
                # next PGS2 header rather than treating it as this request.
                continue
            return payload_size, frame_request_id, sequence
        raise TimeoutError("screenshot frame header not received")

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
            raise ValueError(f"invalid screenshot trailer: {raw!r}")
        return payload_crc


def capture_framebuffer(device, timeout: float = 30.0) -> tuple[int, int, bytes]:
    device.reset_input_buffer()
    request_id = int(time.monotonic_ns() & 0x7FFFFFFF) or 1
    send_command(device, f"screenshot {request_id}")
    reader = _BufferedSerialReader(device)
    payload_size, frame_request_id, sequence = reader.read_header(
        request_id, timeout
    )
    payload = reader.read_exact(payload_size, timeout)
    received_crc = reader.read_trailer(
        frame_request_id, sequence, timeout
    )
    actual_crc = zlib.crc32(payload) & 0xFFFFFFFF
    if received_crc != actual_crc:
        raise ValueError(
            f"screenshot CRC mismatch: received {received_crc:08X}, "
            f"calculated {actual_crc:08X}"
        )
    width, height = 320, 240
    return width, height, payload


def default_output_path() -> pathlib.Path:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return pathlib.Path("captures") / f"playground-{timestamp}.png"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Capture Playground framebuffer as PNG")
    parser.add_argument("--port", required=True, help="USB CDC port, for example COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", type=pathlib.Path, default=None)
    parser.add_argument(
        "--command",
        action="append",
        default=[],
        help="send a firmware command before capture; may be repeated",
    )
    parser.add_argument("--command-delay", type=float, default=0.15)
    parser.add_argument("--settle", type=float, default=0.3)
    parser.add_argument("--timeout", type=float, default=30.0)
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
        device.reset_input_buffer()
        for command in args.command:
            send_command(device, command)
            time.sleep(args.command_delay)
        time.sleep(args.settle)
        width, height, payload = capture_framebuffer(device, args.timeout)
        rows = rgb565be_to_rgb888(payload, width, height)
        write_png(output, width, height, rows)
    except (OSError, TimeoutError, ValueError) as exc:
        print(f"capture failed: {exc}", file=sys.stderr)
        return 1
    finally:
        device.close()

    print(f"Saved {width}x{height} screenshot: {output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
