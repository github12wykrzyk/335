"""Compile and run the real portable C core against deterministic fake game APIs."""
from __future__ import annotations
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AutoLootCoreTests(unittest.TestCase):
    def test_native_state_machine(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            self.skipTest("No C compiler on this runner; native core not verified")
        with tempfile.TemporaryDirectory(prefix="autoloot-core-") as tmp:
            exe = Path(tmp) / ("autoloot-test.exe" if sys.platform == "win32" else "autoloot-test")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                str(ROOT / "src/AutoLoot/autoloot_core.c"),
                str(ROOT / "tests/autoloot_core_harness.c"),
                "-o", str(exe),
            ], check=True, capture_output=True, text=True, timeout=60)
            completed = subprocess.run([str(exe)], check=True, capture_output=True,
                                       text=True, timeout=30)
            self.assertIn("AUTOLOOT_CORE: PASS", completed.stdout)


if __name__ == "__main__":
    unittest.main()
