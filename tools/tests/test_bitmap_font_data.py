import importlib.util
import pathlib
import re
import sys
import unittest


TOOLS_DIR = pathlib.Path(__file__).parents[1]
MODULE_PATH = TOOLS_DIR / "generate_bitmap_font.py"
SPEC = importlib.util.spec_from_file_location("generate_bitmap_font", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
GENERATOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = GENERATOR
SPEC.loader.exec_module(GENERATOR)

FONT_DATA_PATH = TOOLS_DIR.parent / "src" / "ui" / "BitmapFontData.h"
CHARACTERS_PATH = TOOLS_DIR / "font_chars.txt"
SOURCE_ROOT = TOOLS_DIR.parent / "src"


class BitmapFontDataTests(unittest.TestCase):
    def setUp(self) -> None:
        self.expected = [
            ord(character)
            for character in GENERATOR.collect_characters(
                CHARACTERS_PATH, SOURCE_ROOT
            )
        ]
        source = FONT_DATA_PATH.read_text(encoding="utf-8")
        self.glyphs = [
            (int(codepoint, 16), int(advance), bitmap)
            for codepoint, advance, bitmap in re.findall(
                r"^\s+\{0x([0-9A-F]+), (\d+), \{([^}]*)\}\},",
                source,
                re.MULTILINE,
            )
        ]

    def test_both_sizes_contain_the_exact_character_subset(self) -> None:
        count = len(self.expected)
        self.assertEqual(len(self.glyphs), count * 2)
        self.assertEqual([item[0] for item in self.glyphs[:count]], self.expected)
        self.assertEqual([item[0] for item in self.glyphs[count:]], self.expected)

    def test_representative_chinese_glyphs_are_not_blank(self) -> None:
        for character in "开发板屏幕网络连接控制台截图音频麦克风再来一局":
            with self.subTest(character=character):
                matching = [
                    bitmap
                    for codepoint, _, bitmap in self.glyphs
                    if codepoint == ord(character)
                ]
                self.assertEqual(len(matching), 2)
                self.assertTrue(
                    all(any(value != "0x00" for value in bitmap.split(", "))
                        for bitmap in matching)
                )

    def test_small_font_uses_proportional_advances(self) -> None:
        count = len(self.expected)
        small = {
            codepoint: advance
            for codepoint, advance, _ in self.glyphs[:count]
        }
        self.assertLess(small[ord("B")], small[ord("开")])
        self.assertEqual(small[ord("开")], 12)

    def test_small_warning_glyph_fits_inside_16_pixel_line_box(self) -> None:
        count = len(self.expected)
        bitmap_text = next(
            bitmap
            for codepoint, _, bitmap in self.glyphs[:count]
            if codepoint == ord("警")
        )
        values = [int(value, 16) for value in bitmap_text.split(", ")]
        self.assertEqual(len(values), 40)
        rows = [values[index:index + 2] for index in range(0, 32, 2)]
        occupied = [index for index, row in enumerate(rows) if any(row)]
        self.assertGreater(min(occupied), 0)
        self.assertLess(max(occupied), 15)

    def test_ui_string_literals_do_not_reference_missing_glyphs(self) -> None:
        available = set(self.expected)
        used = {
            ord(character)
            for character in GENERATOR.collect_source_characters(SOURCE_ROOT)
        }
        self.assertEqual(used - available, set())

    def test_source_scan_discovers_new_chinese_without_manifest_edits(self) -> None:
        manifest = CHARACTERS_PATH.read_text(encoding="utf-8")
        self.assertNotIn("中", manifest)
        self.assertNotIn("文", manifest)
        self.assertIn(ord("中"), self.expected)
        self.assertIn(ord("文"), self.expected)


if __name__ == "__main__":
    unittest.main()
