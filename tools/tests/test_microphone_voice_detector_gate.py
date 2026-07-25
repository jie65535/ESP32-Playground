import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class MicrophoneVoiceDetectorGateTests(unittest.TestCase):
    def test_three_frame_and_rms_gate(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "microphone_voice_detector_gate_test.exe"
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-Isrc",
                    "src/audio/MicrophoneVoiceDetector.cpp",
                    "tools/tests/microphone_voice_detector_gate_test.cpp",
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
