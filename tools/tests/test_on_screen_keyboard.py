import ast
import re
import string
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
KEYBOARD_SOURCE = ROOT / "src" / "ui" / "OnScreenKeyboard.cpp"
LV_CONF = ROOT / "include" / "lv_conf.h"


def extract_map(source: str, name: str) -> set[str]:
    match = re.search(
        rf"const char\* const {name}\[\] = \{{(.*?)\n\}};",
        source,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"missing keyboard map {name}")

    characters: set[str] = set()
    for token in re.findall(r'"(?:\\.|[^"\\])*"', match.group(1)):
        value = ast.literal_eval(token)
        if value in {"", "\n", "abc", "ABC", "123"}:
            continue
        characters.update(value)
    return characters


class OnScreenKeyboardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = KEYBOARD_SOURCE.read_text(encoding="utf-8")

    def test_maps_cover_printable_ascii(self) -> None:
        available = set()
        for name in ("LOWER_MAP", "UPPER_MAP", "SPECIAL_MAP"):
            available.update(extract_map(self.source, name))
        required = set(
            string.ascii_letters + string.digits + string.punctuation + " "
        )
        self.assertEqual(required - available, set())

    def test_password_text_is_masked_immediately(self) -> None:
        self.assertIn('lv_textarea_set_password_bullet(textArea_, "*")', self.source)
        self.assertIn("lv_textarea_set_password_show_time(textArea_, 0)", self.source)
        self.assertIn("lv_textarea_set_password_mode(textArea_, password)", self.source)

    def test_keyboard_resets_constructor_bottom_alignment(self) -> None:
        align = self.source.index(
            "lv_obj_set_align(keyboard_, LV_ALIGN_TOP_LEFT)"
        )
        position = self.source.index("lv_obj_set_pos(keyboard_, 6, keyboardY)")
        self.assertLess(align, position)

    def test_lvgl_keyboard_dependencies_are_enabled(self) -> None:
        config = LV_CONF.read_text(encoding="utf-8")
        for feature in ("BUTTONMATRIX", "KEYBOARD", "TEXTAREA"):
            self.assertRegex(config, rf"#define LV_USE_{feature}\s+1")

if __name__ == "__main__":
    unittest.main()
