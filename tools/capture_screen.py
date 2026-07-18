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


SCREENSHOT_PREFIX = b"PLAYGROUND_SCREENSHOT "
SCREENSHOT_END = b"PLAYGROUND_SCREENSHOT_END"
SUPPORTED_FORMAT = "RGB565BE"


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


def read_header(device, timeout: float) -> tuple[int, int, str, int]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = device.readline()
        if not line or not line.startswith(SCREENSHOT_PREFIX):
            continue
        fields = line.decode("ascii", errors="strict").strip().split()
        if len(fields) != 6 or fields[0] != "PLAYGROUND_SCREENSHOT" or fields[1] != "1":
            raise ValueError(f"unsupported screenshot header: {line!r}")
        return int(fields[2]), int(fields[3]), fields[4], int(fields[5])
    raise TimeoutError("screenshot header not received")


def read_footer(device, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = device.readline().strip()
        if not line:
            continue
        if line == SCREENSHOT_END:
            return
        raise ValueError(f"unexpected screenshot footer: {line!r}")
    raise TimeoutError("screenshot footer not received")


def capture_framebuffer(device, timeout: float = 10.0) -> tuple[int, int, bytes]:
    device.reset_input_buffer()
    send_command(device, "screenshot")
    width, height, pixel_format, payload_size = read_header(device, timeout)
    if pixel_format != SUPPORTED_FORMAT:
        raise ValueError(f"unsupported pixel format: {pixel_format}")
    expected_size = width * height * 2
    if payload_size != expected_size:
        raise ValueError(
            f"header payload size is {payload_size}, expected {expected_size}"
        )
    payload = read_exact(device, payload_size, timeout)
    read_footer(device, timeout)
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
    parser.add_argument("--timeout", type=float, default=10.0)
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
