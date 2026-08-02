import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class RemoteControlMirrorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.console_header = (
            ROOT / "src" / "apps" / "ConsoleSettingsApp.h"
        ).read_text(encoding="utf-8")
        cls.console_source = (
            ROOT / "src" / "apps" / "ConsoleSettingsApp.cpp"
        ).read_text(encoding="utf-8")
        cls.mirror_source = (
            ROOT / "src" / "services" / "MirrorService.cpp"
        ).read_text(encoding="utf-8")
        cls.mirror_header = (
            ROOT / "src" / "services" / "MirrorService.h"
        ).read_text(encoding="utf-8")
        cls.studio_source = (ROOT / "tools" / "pgos_studio.py").read_text(
            encoding="utf-8"
        )

    def test_remote_page_owns_editable_host_keyboard(self) -> None:
        self.assertIn("OnScreenKeyboard keyboard_", self.console_header)
        self.assertIn("bool onBack(AppContext& context) override", self.console_header)
        self.assertIn("keyboard_.setText", self.console_source)
        self.assertIn("ServerService::validHost", self.console_source)
        self.assertIn("false, 63", self.console_source)

    def test_mirror_send_path_is_bounded_and_nonblocking(self) -> None:
        self.assertIn("MSG_DONTWAIT", self.mirror_source)
        self.assertNotIn("client_.write", self.mirror_source)
        self.assertIn("SEND_STALL_TIMEOUT_MS", self.mirror_header)
        self.assertNotIn("low_internal_heap", self.mirror_source)
        self.assertIn("internalLargestBlock", self.mirror_source)

    def test_studio_waits_for_ack_and_first_frame(self) -> None:
        self.assertIn("def parse_ack_line", self.studio_source)
        self.assertIn("mirror_start_timer", self.studio_source)
        self.assertIn("MIRROR_START_TIMEOUT_MS = 15000", self.studio_source)
        self.assertIn("首帧较慢，设备仍在重试", self.studio_source)
        self.assertIn("镜像首帧已接收", self.studio_source)

    def test_studio_styles_do_not_mix_pixel_fonts_with_point_fonts(self) -> None:
        self.assertNotRegex(self.studio_source, r"font-size:\s*\d+(?:\.\d+)?px")


if __name__ == "__main__":
    unittest.main()
