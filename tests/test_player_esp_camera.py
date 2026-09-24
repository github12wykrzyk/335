"""Pure camera projection regression tests, not client or rendering validation."""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class EspCameraTests(unittest.TestCase):
    def test_portable_camera_projection(self):
        compiler=shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="esp335-camera-") as tmp:
            exe=Path(tmp)/("camera.exe" if sys.platform=="win32" else "camera")
            cmd=[compiler,"-std=c11","-Wall","-Wextra","-Werror",
                 str(ROOT/"src/PlayerESP/player_esp_core.c"),
                 str(ROOT/"src/PlayerESP/player_esp_camera.c"),
                 str(ROOT/"tests/player_esp_camera_harness.c"),
                 "-o",str(exe)]+(["-lm"] if sys.platform!="win32" else [])
            subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn("PLAYER_ESP_CAMERA: PASS",result.stdout)
    def test_335_native_render_domain_not_clip_domain(self):
        host=(ROOT/"src/PlayerESP/player_esp_win32_host.c").read_text()
        self.assertIn("esp335_native_screen_to_ui(screen[0],screen[1],native_rect,&x,&y)",host)
        self.assertNotIn("(screen[0]-min_x)/(max_x-min_x)",host)
        self.assertNotIn("(screen[1]-min_y)/(max_y-min_y)",host)
        self.assertIn("frame+0x330u,&native_rect[0]",host)

if __name__=="__main__":
    unittest.main()
