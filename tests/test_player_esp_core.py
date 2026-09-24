"""Portable ESP core tests do not attest to a native game DLL or gameplay."""
from __future__ import annotations
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class PlayerESPCoreTests(unittest.TestCase):
    def test_native_snapshot_projection_target_filter(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            self.skipTest("No C compiler available; portable ESP core unverified")
        with tempfile.TemporaryDirectory(prefix="esp335-") as tmp:
            exe = Path(tmp) / ("esp335.exe" if sys.platform == "win32" else "esp335")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                str(ROOT / "src/PlayerESP/player_esp_core.c"),
                str(ROOT / "tests/player_esp_core_harness.c"),
                "-o", str(exe),
            ] + (["-lm"] if sys.platform != "win32" else []), check=True, capture_output=True, text=True, timeout=60)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("PLAYER_ESP_CORE: PASS", run.stdout)


if __name__ == "__main__":
    unittest.main()
