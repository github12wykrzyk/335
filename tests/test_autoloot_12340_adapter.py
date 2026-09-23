"""Regression: portable adapter against fake client memory/callbacks only."""
from __future__ import annotations
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AutoLoot12340AdapterTests(unittest.TestCase):
    def test_native_adapter_mock_game(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            self.skipTest("No C compiler; adapter not verified on this runner")
        with tempfile.TemporaryDirectory(prefix="autoloot-native-") as tmp:
            exe = Path(tmp) / ("adapter-test.exe" if sys.platform == "win32" else "adapter-test")
            subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                 str(ROOT / "src/AutoLoot/autoloot_core.c"),
                 str(ROOT / "src/AutoLoot/autoloot_12340_adapter.c"),
                 str(ROOT / "tests/autoloot_12340_adapter_harness.c"),
                 "-o", str(exe)],
                check=True, capture_output=True, text=True, timeout=60,
            )
            result = subprocess.run([str(exe)], check=True,
                                    capture_output=True, text=True, timeout=30)
            self.assertIn("AUTOLOOT_12340_ADAPTER: PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
