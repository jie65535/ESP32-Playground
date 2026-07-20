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
        self.assertEqual(CONSOLE.command_for_key(CONSOLE.KeyEvent("left")), "left")
        self.assertEqual(CONSOLE.command_for_key(CONSOLE.KeyEvent("right")), "right")
        self.assertEqual(CONSOLE.command_for_key(CONSOLE.KeyEvent("enter")), "ok")
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("backspace")), "back"
        )

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
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "4")),
            "page settings",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "5")),
            "page sound",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "6")),
            "page console",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "7")),
            "page rgb",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "8")),
            "page time",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "9")),
            "page snake",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "b")),
            "page breakout",
        )
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "j")),
            "page blackjack",
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
        self.assertEqual(
            CONSOLE.command_for_key(CONSOLE.KeyEvent("character", "w")),
            CONSOLE.WIFI_SETUP_ACTION,
        )

    def test_command_validation(self) -> None:
        self.assertEqual(
            CONSOLE.normalize_commands(
                ["UP", "page display", "page breakout", "page blackjack", "status"]
            ),
            ["up", "page display", "page breakout", "page blackjack", "status"],
        )
        with self.assertRaises(ValueError):
            CONSOLE.normalize_commands(["alarm"])

    def test_wifi_commands_preserve_case_sensitive_values(self) -> None:
        self.assertEqual(
            CONSOLE.normalize_commands(
                [
                    "WIFI SCAN",
                    "wifi on",
                    "wifi select 2",
                    "wifi ssid HomeNet",
                    "wifi password AbCd1234",
                    "server set 192.168.1.4 19000",
                    "mirror status",
                    "bench upload 4194304",
                    "time set 2026-07-19 12:34:56",
                    "i2c scan",
                ]
            ),
            [
                "wifi scan",
                "wifi on",
                "wifi select 2",
                "wifi ssid HomeNet",
                "wifi password AbCd1234",
                "server set 192.168.1.4 19000",
                "mirror status",
                "bench upload 4194304",
                "time set 2026-07-19 12:34:56",
                "i2c scan",
            ],
        )
        self.assertEqual(
            CONSOLE.command_for_log("wifi password AbCd1234"),
            "wifi password <hidden>",
        )


if __name__ == "__main__":
    unittest.main()
