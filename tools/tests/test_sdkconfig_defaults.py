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

    def test_wifi_lwip_prefers_psram_and_internal_memory_is_reserved(self) -> None:
        config = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
        self.assertIn("CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y", config)
        self.assertIn("CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024", config)
        self.assertIn("CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768", config)


if __name__ == "__main__":
    unittest.main()
