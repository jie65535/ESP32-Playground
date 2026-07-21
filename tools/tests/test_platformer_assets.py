import importlib.util
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "convert_platformer_assets.py"
BACKGROUND_SCRIPT = ROOT / "tools" / "convert_mario_level_background.py"


def load_converter():
    spec = importlib.util.spec_from_file_location("platformer_asset_converter", SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError("unable to load converter")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_background_converter():
    spec = importlib.util.spec_from_file_location(
        "mario_level_background_converter", BACKGROUND_SCRIPT
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("unable to load background converter")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PlatformerAssetConverterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.converter = load_converter()

    def test_rgb565_and_safe_name(self):
        self.assertEqual(self.converter.rgb565(255, 0, 0), 0xF800)
        self.assertEqual(self.converter.rgb565(0, 255, 0), 0x07E0)
        self.assertEqual(self.converter.rgb565(0, 0, 255), 0x001F)
        self.assertEqual(self.converter.safe_name("goomba-1"), "goomba_1")

    def test_colorkey_and_source_alpha_are_combined(self):
        image = Image.new("RGBA", (3, 1))
        image.putdata(
            [
                (255, 0, 0, 255),
                (255, 0, 0, 255),
                (12, 34, 56, 0),
            ]
        )
        width, height, pixels, alpha = self.converter.read_pixel_frame(
            image, (0, 0, 3, 1), (255, 0, 0)
        )
        self.assertEqual((width, height), (3, 1))
        self.assertEqual(len(pixels), width * height)
        self.assertEqual(len(alpha), width * height)
        self.assertEqual(alpha, [0, 0, 0])

    def test_generate_validates_lengths_and_emits_frame_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sprites").mkdir()
            (root / "img").mkdir()
            sheet = Image.new("RGBA", (16, 16), (10, 20, 30, 255))
            sheet.putpixel((0, 0), (10, 20, 30, 0))
            sheet.save(root / "img" / "sheet.png")
            (root / "sprites" / "Sample.json").write_text(
                json.dumps(
                    {
                        "spriteSheetURL": "./img/sheet.png",
                        "type": "background",
                        "sprites": [{"name": "sample", "x": 0, "y": 0}],
                    }
                ),
                encoding="utf-8",
            )
            output = root / "assets.generated.h"
            self.converter.generate(
                root,
                output,
                manifests=("Sample.json",),
                wanted=("sample",),
            )
            text = output.read_text(encoding="utf-8")
            self.assertIn('"sample", 16, 16', text)
            self.assertIn("sample_image_data", text)
            self.assertNotIn("sample_pixels", text)
            self.assertNotIn("sample_alpha", text)

    def test_generated_reference_assets_have_expected_shape(self):
        compiler = shutil.which("g++")
        generated = ROOT / "src" / "games" / "PlatformerSpriteAssets.generated.h"
        if compiler is None or not generated.exists():
            self.skipTest("generated asset header or g++ is unavailable")

        source = r'''
#include "games/PlatformerSpriteAssets.generated.h"
#include <cstddef>
#include <cstring>

int main() {
    using namespace pgos::platformer_assets;
    constexpr std::size_t frameCount = sizeof(all) / sizeof(all[0]);
    if (frameCount != 39U) return 1;
    bool hasTransparent = false;
    bool hasOpaque = false;
    for (const Frame* frame : all) {
        if (frame == nullptr || frame->name == nullptr) return 2;
        const bool koopa = std::strcmp(frame->name, "koopa-1") == 0 ||
                           std::strcmp(frame->name, "koopa-2") == 0;
        const bool bigMario = std::strncmp(frame->name, "mario_big_", 10) == 0;
        const uint16_t expectedWidth = koopa ? 17U : 16U;
        const uint16_t expectedHeight = (koopa || bigMario) ? 32U : 16U;
        const uint32_t expectedCount =
            static_cast<uint32_t>(expectedWidth) * expectedHeight;
        if (frame->width != expectedWidth ||
            frame->height != expectedHeight ||
            frame->pixelCount() != expectedCount || !frame->valid() ||
            frame->imageDataSize != expectedCount * 3U) return 2;
        for (std::size_t i = 0; i < expectedCount; ++i) {
            hasTransparent |= frame->opacityAt(i) == 0U;
            hasOpaque |= frame->opacityAt(i) == 255U;
        }
    }
    return hasTransparent && hasOpaque ? 0 : 3;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "asset_check.cpp"
            executable = directory / "asset_check.exe"
            source_path.write_text(source, encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c++17", "-Isrc", str(source_path), "-o", str(executable)],
                cwd=ROOT,
                check=True,
                capture_output=True,
            )
            subprocess.run([str(executable)], cwd=ROOT, check=True)

    def test_generated_extra_assets_have_expected_shape(self):
        compiler = shutil.which("g++")
        generated = ROOT / "src" / "games" / "PlatformerExtraAssets.generated.h"
        if compiler is None or not generated.exists():
            self.skipTest("generated extra asset header or g++ is unavailable")

        source = r'''
#include "games/PlatformerExtraAssets.generated.h"
#include <cstddef>

int main() {
    using namespace pgos::platformer_extra_assets;
    constexpr std::size_t frameCount = sizeof(all) / sizeof(all[0]);
    if (frameCount != 36U) return 1;
    bool hasSmall = false;
    bool hasLarge = false;
    for (const Frame* frame : all) {
        if (frame == nullptr || frame->name == nullptr ||
            frame->width == 0 || frame->height == 0) return 2;
        const uint32_t expected =
            static_cast<uint32_t>(frame->width) * frame->height;
        if (frame->pixelCount() != expected || !frame->valid() ||
            frame->imageDataSize != expected * 3U) return 3;
        hasSmall |= frame->width <= 8U;
        hasLarge |= frame->height >= 32U;
    }
    return hasSmall && hasLarge ? 0 : 4;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "extra_asset_check.cpp"
            executable = directory / "extra_asset_check.exe"
            source_path.write_text(source, encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c++17", "-Isrc", str(source_path), "-o", str(executable)],
                cwd=ROOT,
                check=True,
                capture_output=True,
            )
            subprocess.run([str(executable)], cwd=ROOT, check=True)

    def test_background_converter_uses_explicit_paths_and_chunks(self):
        converter = load_background_converter()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "level.png"
            output = root / "nested" / "background.generated.h"
            image = Image.new("RGB", (5, 2))
            image.putdata(
                [
                    (255, 0, 0),
                    (0, 255, 0),
                    (0, 0, 255),
                    (255, 255, 255),
                    (0, 0, 0),
                ]
                * 2
            )
            image.save(source)
            self.assertEqual(converter.generate(source, output, chunk_width=2), 3)
            text = output.read_text(encoding="utf-8")
            self.assertIn('"level_0", 2, 2', text)
            self.assertIn('"level_2", 1, 2', text)
            self.assertIn("level_2_image_data", text)


if __name__ == "__main__":
    unittest.main()
