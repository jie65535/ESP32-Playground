"""PlatformIO pre-build hook for the TinyLM corpus-derived story font."""

from __future__ import annotations

import importlib.util
from pathlib import Path

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))  # type: ignore[name-defined]
GENERATOR_PATH = PROJECT_DIR / "tools" / "generate_tinylm_font.py"
OUTPUT_PATH = PROJECT_DIR / "src" / "ui" / "TinyLmFontData.h"
BDF_PATH = (
    PROJECT_DIR
    / "tools"
    / "vendor-fonts"
    / "fusion-pixel-12px-proportional-zh_hans.bdf"
)
RAW_DIR = (
    PROJECT_DIR
    / "src"
    / "third_party"
    / "esp32_ai"
    / "data"
    / "raw"
    / "TinyStories-Zh-1M"
)
PARQUETS = [
    RAW_DIR / "train-00000-of-00002.parquet",
    RAW_DIR / "train-00001-of-00002.parquet",
]


def load_generator():
    spec = importlib.util.spec_from_file_location(
        "pgos_tinylm_font_generator", GENERATOR_PATH
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load TinyLM font generator: {GENERATOR_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


inputs = [GENERATOR_PATH, BDF_PATH, *PARQUETS]
missing_inputs = [path for path in inputs if not path.exists()]
if missing_inputs:
    if not OUTPUT_PATH.exists():
        names = ", ".join(str(path) for path in missing_inputs)
        raise RuntimeError(
            "TinyLM font data is missing and cannot be regenerated; missing: "
            + names
        )
    print("TinyLM raw font inputs unavailable; using checked-in font data")
else:
    newest_input = max(path.stat().st_mtime_ns for path in inputs)
    output_time = OUTPUT_PATH.stat().st_mtime_ns if OUTPUT_PATH.exists() else 0
    if output_time < newest_input:
        load_generator().generate(PARQUETS, BDF_PATH, OUTPUT_PATH)
    else:
        print("TinyLM story font is current")
