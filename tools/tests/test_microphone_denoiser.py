import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEEX_ROOT = ROOT / "src" / "third_party" / "speexdsp"


class MicrophoneDenoiserTests(unittest.TestCase):
    def test_fixed_point_spectral_denoiser(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")

        c_sources = [
            "fftwrap.c",
            "filterbank.c",
            "kiss_fft.c",
            "kiss_fftr.c",
            "mdf.c",
            "preprocess.c",
        ]
        with tempfile.TemporaryDirectory() as directory:
            objects = []
            for source in c_sources:
                output = Path(directory) / f"{source}.o"
                command = [
                    shutil.which("gcc") or "gcc",
                    "-std=c11",
                    "-DHAVE_CONFIG_H=1",
                    f"-I{SPEEX_ROOT / 'include'}",
                    f"-I{SPEEX_ROOT / 'src'}",
                    "-c",
                    str(SPEEX_ROOT / "src" / source),
                    "-o",
                    str(output),
                ]
                subprocess.run(command, cwd=ROOT, check=True, capture_output=True)
                objects.append(str(output))

            executable = Path(directory) / "microphone_denoiser_test.exe"
            command = [
                compiler,
                "-std=c++17",
                "-Isrc",
                f"-I{SPEEX_ROOT / 'include'}",
                "src/audio/MicrophoneDenoiser.cpp",
                "tools/tests/microphone_denoiser_test.cpp",
                *objects,
                "-o",
                str(executable),
            ]
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True)
            subprocess.run([str(executable)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
