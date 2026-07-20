import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SDKCONFIG_DEFAULTS = ROOT / "sdkconfig.defaults"


class SdkconfigDefaultsTests(unittest.TestCase):
    def test_wifi_event_loop_has_stack_headroom(self) -> None:
        config = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
        self.assertIn(
            "CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096", config
        )


if __name__ == "__main__":
    unittest.main()
