#!/usr/bin/env python3
"""Small USB CDC console for ESP32 Playground.

Arrow keys and Enter are mapped to the same line commands used by the
firmware. The screenshot action temporarily pauses the log reader so the raw
RGB565 frame cannot be mixed with human-readable serial output.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import sys
import threading
import time
from dataclasses import dataclass
from typing import Iterable, Protocol


TOOLS_DIR = pathlib.Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import capture_screen


VALID_COMMANDS = (
    "up",
    "down",
    "ok",
    "status",
    "screenshot",
    "color_test",
    "help",
    "page system",
    "page display",
    "page network",
)

CAPTURE_ACTION = "__capture_screenshot__"

SHORTCUTS = {
    "0": "page system",
    "1": "page system",
    "2": "page display",
    "3": "page network",
    "c": "color_test",
    "r": "status",
    "h": "help",
    "?": "help",
    "s": CAPTURE_ACTION,
}


class Transport(Protocol):
    def send(self, command: str) -> None: ...

    def capture(self) -> pathlib.Path | None: ...

    def close(self) -> None: ...


class DryRunTransport:
    def send(self, command: str) -> None:
        print(f"[DRY-RUN] send: {command}")

    def capture(self) -> pathlib.Path | None:
        print("[DRY-RUN] capture screenshot and copy image to clipboard")
        return None

    def close(self) -> None:
        return


class SerialTransport:
    def __init__(self, port: str, baud: int) -> None:
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError(
                "pyserial is required; run: python -m pip install -r tools/requirements.txt"
            ) from exc

        self._serial = serial.Serial()
        self._serial.port = port
        self._serial.baudrate = baud
        self._serial.timeout = 0.1
        # Native USB Serial/JTAG can reset when DTR/RTS are asserted.
        self._serial.dtr = False
        self._serial.rts = False
        self._serial.open()
        self._stop = threading.Event()
        self._capture_requested = threading.Event()
        self._reader_paused = threading.Event()
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def send(self, command: str) -> None:
        self._serial.write((command + "\n").encode("ascii"))
        self._serial.flush()
        print(f"[HOST] {command}")

    def capture(self) -> pathlib.Path | None:
        output = capture_screen.default_output_path()
        self._capture_requested.set()
        try:
            if not self._reader_paused.wait(timeout=1.0):
                raise TimeoutError("serial log reader did not pause for screenshot")
            width, height, payload = capture_screen.capture_framebuffer(self._serial)
            rows = capture_screen.rgb565be_to_rgb888(payload, width, height)
            capture_screen.write_png(output, width, height, rows)
            dib = capture_screen.rgb565be_to_dib(payload, width, height)
            capture_screen.copy_dib_to_clipboard(dib)
        except (MemoryError, OSError, RuntimeError, TimeoutError, ValueError) as exc:
            print(f"\n[HOST] screenshot failed: {exc}")
            return None
        finally:
            self._capture_requested.clear()

        resolved = output.resolve()
        print(f"\n[HOST] screenshot saved: {resolved}")
        print("[HOST] image copied to clipboard")
        return resolved

    def close(self) -> None:
        self._stop.set()
        self._capture_requested.clear()
        self._reader.join(timeout=0.5)
        self._serial.close()

    def _read_loop(self) -> None:
        while not self._stop.is_set():
            if self._capture_requested.is_set():
                self._reader_paused.set()
                while self._capture_requested.is_set() and not self._stop.is_set():
                    self._stop.wait(0.01)
                self._reader_paused.clear()
                continue
            try:
                line = self._serial.readline()
            except Exception as exc:
                print(f"\n[DEVICE] read error: {exc}")
                self._stop.set()
                return
            if line:
                print(f"\n[DEVICE] {line.decode('utf-8', errors='replace').rstrip()}")


@dataclass(frozen=True)
class KeyEvent:
    name: str
    character: str = ""


def command_for_key(event: KeyEvent) -> str | None:
    if event.name == "up":
        return "up"
    if event.name == "down":
        return "down"
    if event.name == "enter":
        return "ok"
    if event.name == "character":
        return SHORTCUTS.get(event.character.lower())
    return None


def normalize_commands(commands: Iterable[str]) -> list[str]:
    normalized = []
    for command in commands:
        value = command.strip().lower()
        if value not in VALID_COMMANDS:
            raise ValueError(f"unsupported command: {command}")
        normalized.append(value)
    return normalized


def list_ports() -> list[str]:
    try:
        from serial.tools import list_ports
    except ImportError as exc:
        raise RuntimeError(
            "pyserial is required; run: python -m pip install -r tools/requirements.txt"
        ) from exc
    return [f"{port.device}\t{port.description}" for port in list_ports.comports()]


def print_controls() -> None:
    print()
    print("ESP32 Playground USB console")
    print("  1/0 system   2 display   3 network placeholder")
    print("  C color test   R status   S screenshot + clipboard")
    print("  Arrow Up/Down and Enter are optional convenience keys")
    print("  Full commands are accepted with --command; H help, Q quit")
    print()


def read_key() -> KeyEvent:
    if os.name == "nt":
        import msvcrt

        char = msvcrt.getwch()
        if char in ("\x00", "\xe0"):
            extended = msvcrt.getwch()
            if extended == "H":
                return KeyEvent("up")
            if extended == "P":
                return KeyEvent("down")
            return KeyEvent("ignored")
        if char in ("\r", "\n"):
            return KeyEvent("enter")
        if char.lower() == "q":
            return KeyEvent("quit")
        return KeyEvent("character", char)

    import termios
    import tty

    fd = sys.stdin.fileno()
    original = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        char = sys.stdin.read(1)
        if char == "\x1b":
            suffix = sys.stdin.read(2)
            if suffix == "[A":
                return KeyEvent("up")
            if suffix == "[B":
                return KeyEvent("down")
            return KeyEvent("ignored")
        if char in ("\r", "\n"):
            return KeyEvent("enter")
        if char.lower() == "q":
            return KeyEvent("quit")
        return KeyEvent("character", char)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, original)


def run_interactive(transport: Transport) -> None:
    print_controls()
    while True:
        event = read_key()
        if event.name == "quit":
            print("\nExiting.")
            return
        command = command_for_key(event)
        if command is None:
            continue
        if command == "help":
            print_controls()
        elif command == CAPTURE_ACTION:
            transport.capture()
        else:
            transport.send(command)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="ESP32 Playground USB CDC console")
    parser.add_argument("--port", help="serial port, for example COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--list", action="store_true", help="list available ports")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--command", action="append", default=[])
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.list:
        try:
            for description in list_ports():
                print(description)
        except RuntimeError as exc:
            print(exc, file=sys.stderr)
            return 2
        return 0

    try:
        commands = normalize_commands(args.command)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 2

    if args.dry_run:
        transport: Transport = DryRunTransport()
    else:
        if not args.port:
            print("--port is required unless --dry-run or --list is used", file=sys.stderr)
            return 2
        try:
            transport = SerialTransport(args.port, args.baud)
        except (RuntimeError, OSError) as exc:
            print(exc, file=sys.stderr)
            return 2

    try:
        if commands:
            for command in commands:
                if command == "screenshot":
                    transport.capture()
                else:
                    transport.send(command)
                time.sleep(0.05)
        else:
            run_interactive(transport)
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        transport.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
