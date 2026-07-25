import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class MicrophoneVoiceEnhancerTests(unittest.TestCase):
    def test_non_voice_frame_releases_gain_immediately(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "microphone_voice_enhancer_test.exe"
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-Isrc",
                    "src/audio/MicrophoneVoiceEnhancer.cpp",
                    "tools/tests/microphone_voice_enhancer_test.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
            )
            subprocess.run([str(executable)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
