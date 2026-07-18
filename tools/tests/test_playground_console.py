import importlib.util
import pathlib
import sys
import unittest


MODULE_PATH = pathlib.Path(__file__).parents[1] / "playground_console.py"
SPEC = importlib.util.spec_from_file_location("playground_console", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
CONSOLE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CONSOLE
SPEC.loader.exec_module(CONSOLE)


class ConsoleMappingTests(unittest.TestCase):
    def test_arrow_and_enter_map_to_firmware_commands(self) -> None:
        self.assertEqual(CONSOLE.command_for_key(CONSOLE.KeyEvent("up")), "up")
        self.assertEqual(CONSOLE.command_for_key(CONSOLE.KeyEvent("down")), "down")
        self.assertEqual(CONSOLE.command_for_key(CONSOLE.KeyEvent("enter")), "ok")

    def test_shortcuts_cover_pages_color_and_capture(self) -> None:
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "1")),
            "page system",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "2")),
            "page display",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "c")),
            "color_test",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "s")),
            CONSOLE.CAPTURE_ACTION,
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "r")),
            "status",
        )

    def test_command_validation(self) -> None:
        self.assertEqual(
            CONSOLE.normalize_commands(["UP", "page display", "status"]),
            ["up", "page display", "status"],
        )
        with self.assertRaises(ValueError):
            CONSOLE.normalize_commands(["alarm"])


if __name__ == "__main__":
    unittest.main()
