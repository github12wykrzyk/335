"""The 112-style rewrite uses a separate native device-domain conversion and
OS pixel labels; legacy 335 Lua/D3D renderers are not part of the new build."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class PortTests(unittest.TestCase):
    def test_real_112_style_geometry(self):
        compiler=shutil.which("clang") or shutil.which("gcc")
        if not compiler:self.skipTest("C compiler absent")
        with tempfile.TemporaryDirectory() as tmp:
            executable=Path(tmp)/"geometry"
            cmd=[compiler,"-std=c11","-Wall","-Wextra","-Werror",
                 str(ROOT/"src/PlayerESP112Port/esp112_geometry.c"),
                 str(ROOT/"tests/esp112_geometry_harness.c"),
                 "-o",str(executable),"-lm"]
            subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
            run=subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("ESP112_335_GEOMETRY: PASS",run.stdout)
    def test_os_overlay_not_lua(self):
        g=(ROOT/"src/PlayerESP112Port/esp112_overlay.c").read_text()
        self.assertIn("CreateWindowExA",g)
        self.assertIn("WS_EX_LAYERED",g)
        self.assertIn("ClientToScreen", (ROOT/"src/PlayerESP112Port/esp112_host335.c").read_text())
        self.assertNotIn("UIParent",g)
if __name__=="__main__":unittest.main()
