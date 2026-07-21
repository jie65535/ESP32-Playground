#!/usr/bin/env python3
"""Capture and stitch the Platformer fixed-step map-test panorama.

The firmware keeps Mario and all checkpoint enemies stationary while
``platformer mapnext`` advances the camera by 256 world pixels. Each 320px
frame therefore overlaps the previous frame by 64px and the final panorama is
exactly 3392px wide.
"""

from __future__ import annotations

import argparse
import pathlib
import sys
import time

from PIL import Image

TOOLS_DIR = pathlib.Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import capture_screen


FRAME_WIDTH = 320
FRAME_HEIGHT = 240
WORLD_TOP = 48
WORLD_HEIGHT = FRAME_HEIGHT - WORLD_TOP
OVERLAP = 64
STEP_WIDTH = FRAME_WIDTH - OVERLAP
WORLD_WIDTH = 3392
FRAME_COUNT = 1 + (WORLD_WIDTH - FRAME_WIDTH + STEP_WIDTH - 1) // STEP_WIDTH


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture and stitch the Platformer map-test panorama"
    )
    parser.add_argument("--port", required=True, help="USB CDC port, for example COM3")
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("captures/platformer/maptest/platformer-map-panorama.png"),
    )
    parser.add_argument("--settle", type=float, default=0.25)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument(
        "--retries",
        type=int,
        default=2,
        help="retry a transient screenshot failure without advancing the map",
    )
    return parser


def capture_frame_with_retry(device, timeout: float, retries: int):
    last_error: Exception | None = None
    for attempt in range(max(0, retries) + 1):
        try:
            return capture_screen.capture_framebuffer(device, timeout)
        except (OSError, TimeoutError, ValueError) as exc:
            last_error = exc
            if attempt >= max(0, retries):
                break
            time.sleep(0.15)
    assert last_error is not None
    raise last_error


def stitch_segments(segment_dir: pathlib.Path) -> Image.Image:
    panorama = Image.new("RGB", (WORLD_WIDTH, WORLD_HEIGHT))
    for index in range(FRAME_COUNT):
        segment_path = segment_dir / f"map-{index:02d}.png"
        with Image.open(segment_path) as segment_image:
            segment = segment_image.convert("RGB")
        world = segment.crop((0, WORLD_TOP, FRAME_WIDTH, FRAME_HEIGHT))
        if index == 0:
            panorama.paste(world, (0, 0))
        else:
            visible = world.crop((OVERLAP, 0, FRAME_WIDTH, WORLD_HEIGHT))
            panorama.paste(
                visible,
                (FRAME_WIDTH + (index - 1) * STEP_WIDTH, 0),
            )
    return panorama


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    output = args.output
    segment_dir = output.parent / "segments"
    segment_dir.mkdir(parents=True, exist_ok=True)

    device = capture_screen.open_serial(args.port, 115200)
    try:
        capture_screen.send_command(device, "platformer maptest")
        time.sleep(args.settle)
        for index in range(FRAME_COUNT):
            width, height, payload = capture_frame_with_retry(
                device, args.timeout, args.retries
            )
            if (width, height) != (FRAME_WIDTH, FRAME_HEIGHT):
                raise ValueError(f"unexpected framebuffer size: {width}x{height}")
            segment_path = segment_dir / f"map-{index:02d}.png"
            capture_screen.write_png(
                segment_path,
                width,
                height,
                capture_screen.rgb565be_to_rgb888(payload, width, height),
            )
            print(f"Saved {segment_path}")
            if index + 1 < FRAME_COUNT:
                capture_screen.send_command(device, "platformer mapnext")
                time.sleep(args.settle)
    except (OSError, TimeoutError, ValueError) as exc:
        print(f"map capture failed: {exc}", file=sys.stderr)
        return 1
    finally:
        device.close()

    panorama = stitch_segments(segment_dir)
    output.parent.mkdir(parents=True, exist_ok=True)
    panorama.save(output)
    print(f"Saved {panorama.width}x{panorama.height} panorama: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
