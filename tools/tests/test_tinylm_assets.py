import csv
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FONT_DATA = ROOT / "src" / "ui" / "TinyLmFontData.h"
VOCAB_DATA = (
    ROOT
    / "src"
    / "third_party"
    / "esp32_ai"
    / "firmware"
    / "esp32_llm"
    / "vocab.h"
)
MODEL_DATA = (
    ROOT
    / "src"
    / "third_party"
    / "esp32_ai"
    / "firmware"
    / "model"
    / "model.bin"
)
PARTITIONS = ROOT / "partitions_16MB.csv"


def parse_c_array(source: str, name: str) -> list[int]:
    match = re.search(
        rf"\b{name}\b[^=]*=\s*\{{(.*?)\}};", source, re.DOTALL
    )
    if match is None:
        raise AssertionError(f"missing C array: {name}")
    return [int(value) for value in re.findall(r"\b\d+\b", match.group(1))]


class TinyLmAssetTests(unittest.TestCase):
    def test_story_font_has_sorted_corpus_coverage(self) -> None:
        source = FONT_DATA.read_text(encoding="utf-8")
        glyphs = [
            (int(codepoint, 16), int(advance), bitmap)
            for codepoint, advance, bitmap in re.findall(
                r"^\s+\{0x([0-9A-F]+), (\d+), \{([^}]*)\}\},",
                source,
                re.MULTILINE,
            )
        ]
        codepoints = [item[0] for item in glyphs]
        self.assertGreaterEqual(len(glyphs), 3950)
        self.assertEqual(codepoints, sorted(set(codepoints)))
        for character in "从前有一只小兔子森林宝石朋友女孩�AZ09，。！？":
            with self.subTest(character=character):
                self.assertIn(ord(character), codepoints)
        self.assertTrue(all(len(re.findall(r"0x[0-9A-F]{2}", item[2])) == 40
                            for item in glyphs))

    def test_vocab_matches_service_decode_buffer(self) -> None:
        source = VOCAB_DATA.read_text(encoding="ascii")
        self.assertRegex(source, r"#define VOCAB_N 8192\b")
        blob = parse_c_array(source, "VOCAB_BLOB")
        offsets = parse_c_array(source, "VOCAB_OFF")
        self.assertEqual(len(offsets), 8193)
        self.assertEqual(offsets[0], 0)
        self.assertEqual(offsets[-1], len(blob))
        self.assertEqual(offsets, sorted(offsets))
        longest = max(right - left for left, right in zip(offsets, offsets[1:]))
        self.assertLessEqual(longest, 64)

    def test_model_binary_matches_exported_size_when_present(self) -> None:
        if not MODEL_DATA.exists():
            self.skipTest("ignored model.bin is not available in this checkout")
        self.assertEqual(MODEL_DATA.stat().st_size, 2_947_012)
        self.assertEqual(MODEL_DATA.read_bytes()[:4], b"1ELP")

    def test_partition_layout_keeps_dual_ota_and_model_space(self) -> None:
        rows: dict[str, tuple[int, int]] = {}
        with PARTITIONS.open(encoding="utf-8", newline="") as handle:
            for row in csv.reader(
                line for line in handle if line.strip() and not line.startswith("#")
            ):
                name = row[0].strip()
                rows[name] = (int(row[3].strip(), 0), int(row[4].strip(), 0))

        self.assertEqual(rows["app0"], (0x10000, 0x400000))
        self.assertEqual(rows["app1"], (0x410000, 0x400000))
        self.assertEqual(rows["model"], (0x810000, 0x400000))
        self.assertGreaterEqual(rows["model"][1], 2_947_012)
        self.assertEqual(rows["spiffs"], (0xC10000, 0x3E0000))
        ordered = sorted((offset, offset + size, name)
                         for name, (offset, size) in rows.items())
        for previous, current in zip(ordered, ordered[1:]):
            self.assertLessEqual(previous[1], current[0],
                                 f"{previous[2]} overlaps {current[2]}")
        self.assertLessEqual(max(end for _, end, _ in ordered), 0x1000000)


if __name__ == "__main__":
    unittest.main()
