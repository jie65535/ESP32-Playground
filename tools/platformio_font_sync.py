"""PlatformIO pre-build hook for keeping the generated bitmap subset current."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import re

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))  # type: ignore[name-defined]
TOOLS_DIR = PROJECT_DIR / "tools"
GENERATOR_PATH = TOOLS_DIR / "generate_bitmap_font.py"
FONT_DATA_PATH = PROJECT_DIR / "src" / "ui" / "BitmapFontData.h"
CHARACTERS_PATH = TOOLS_DIR / "font_chars.txt"
SOURCE_ROOT = PROJECT_DIR / "src"
SMALL_BDF = TOOLS_DIR / "vendor-fonts" / "fusion-pixel-12px-proportional-zh_hans.bdf"
BOLD_BDF = TOOLS_DIR / "vendor-fonts" / "fusion-bold-pixel-12px-proportional-zh_hans.bdf"


def load_generator():
    spec = importlib.util.spec_from_file_location("pgos_bitmap_font_generator", GENERATOR_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load bitmap font generator: {GENERATOR_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def generated_codepoints(path: Path) -> set[int]:
    if not path.exists():
        return set()
    source = path.read_text(encoding="utf-8")
    return {
        int(value, 16)
        for value in re.findall(r"^\s+\{0x([0-9A-F]+),", source, re.MULTILINE)
    }


generator = load_generator()
expected = {
    ord(character)
    for character in generator.collect_characters(CHARACTERS_PATH, SOURCE_ROOT)
}
available = generated_codepoints(FONT_DATA_PATH)

if expected != available:
    missing = expected - available
    if not SMALL_BDF.exists() or not BOLD_BDF.exists():
        names = " ".join(chr(value) for value in sorted(missing)) or "--"
        raise RuntimeError(
            "BitmapFontData.h is out of date and local BDF sources are missing. "
            f"Missing characters: {names}"
        )
    generator.generate(
        SMALL_BDF,
        BOLD_BDF,
        CHARACTERS_PATH,
        FONT_DATA_PATH,
        SOURCE_ROOT,
    )
else:
    print("Bitmap font subset is current")
