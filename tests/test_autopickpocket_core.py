"""Compile and execute the portable AutoPickPocket C core on Linux CI.
This does not certify or generate a runnable WoW game DLL.
"""
from __future__ import annotations
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class AutoPickPocketCoreTests(unittest.TestCase):
    def test_native_core(self):
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            if os.name == "nt":
                self.skipTest("Windows: portable C is compiled in Linux verifier, game adapter is not ready")
            self.fail("No C compiler available for AutoPickPocket core verification")
        with tempfile.TemporaryDirectory(prefix="pp335-core-") as tmp:
            binary = str(Path(tmp) / ("autopickpocket_test.exe" if os.name == "nt" else "autopickpocket_test"))
            cmd = [compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   str(ROOT / "src/AutoPickPocket/autopickpocket_core.c"),
                   str(ROOT / "tests/autopickpocket_core_test.c"), "-o", binary]
            compile_result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            run = subprocess.run([binary], cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("AutoPickPocket portable core tests: PASS", run.stdout)

if __name__ == "__main__":
    unittest.main()
