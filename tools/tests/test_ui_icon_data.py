import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ICON_DATA = ROOT / "src" / "ui" / "UiIconData.inc"
ICON_HEADER = ROOT / "src" / "ui" / "UiIcons.h"


class UiIconDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = ICON_DATA.read_text(encoding="ascii")
        cls.header = ICON_HEADER.read_text(encoding="utf-8")

    def test_each_declared_icon_has_a_24_by_24_a8_bitmap(self):
        enum_body = re.search(
            r"enum class UiIcon[^\{]*\{(.*?)\};", self.header, re.DOTALL
        ).group(1)
        enum_names = re.findall(r"^\s+([A-Za-z0-9_]+),", enum_body, re.MULTILINE)
        arrays = re.findall(
            r"static const uint8_t ICON_([A-Z0-9_]+)_DATA\[\] = \{(.*?)\};",
            self.data,
            re.DOTALL,
        )
        descriptors = re.findall(
            r"static const lv_image_dsc_t ICON_([A-Z0-9_]+) = \{(.*?)\n\};",
            self.data,
            re.DOTALL,
        )

        self.assertEqual(len(arrays), len(enum_names))
        self.assertEqual(len(descriptors), len(enum_names))
        for _, body in arrays:
            self.assertEqual(len(re.findall(r"0x[0-9A-F]{2}", body)), 24 * 24)
        for _, body in descriptors:
            self.assertIn("LV_COLOR_FORMAT_A8", body)
            self.assertIn("24, 24, 24", body)

    def test_generated_asset_keeps_source_and_license_metadata(self):
        self.assertIn("Lucide Static 0.468.0", self.data)
        self.assertTrue((ROOT / "licenses" / "Lucide-ISC.txt").is_file())


if __name__ == "__main__":
    unittest.main()
