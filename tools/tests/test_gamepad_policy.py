import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).parents[2]
SOURCE = ROOT / "tools" / "tests" / "cpp" / "gamepad_policy_test.cpp"


class GamepadPolicyTests(unittest.TestCase):
    def test_activity_and_reconnect_policy(self) -> None:
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is not installed")
        with tempfile.TemporaryDirectory() as temporary_directory:
            executable = pathlib.Path(temporary_directory) / "gamepad_policy_test.exe"
            subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "src"),
                    str(SOURCE),
                    "-o",
                    str(executable),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
