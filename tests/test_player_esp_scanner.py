"""A fake memory map exercises exact-client scanner contracts, not a game DLL."""
from __future__ import annotations
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

class PlayerESPScannerTests(unittest.TestCase):
    def test_scanner_snapshot_lifetime(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            self.skipTest("No C compiler")
        with tempfile.TemporaryDirectory(prefix="esp335-scan-") as tmp:
            exe = Path(tmp) / ("esp335-scan.exe" if sys.platform=="win32" else "esp335-scan")
            args = [compiler,"-std=c11","-Wall","-Wextra","-Werror",
                str(ROOT/"src/PlayerESP/player_esp_core.c"),
                str(ROOT/"src/PlayerESP/player_esp_scanner.c"),
                str(ROOT/"tests/player_esp_scanner_harness.c"),
                "-o",str(exe)] + (["-lm"] if sys.platform!="win32" else [])
            subprocess.run(args, check=True, capture_output=True, text=True, timeout=60)
            run = subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("PLAYER_ESP_SCANNER: PASS",run.stdout)

if __name__=="__main__": unittest.main()
