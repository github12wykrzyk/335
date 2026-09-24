"""The 112-style rewrite uses a separate native device-domain conversion and
OS pixel labels; legacy 335 Lua/D3D renderers are not part of the new build."""
import shutil
import sys
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
                 "-o",str(executable)]+(["-lm"] if sys.platform!="win32" else [])
            subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
            run=subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("ESP112_335_GEOMETRY: PASS",run.stdout)
    def test_stable_npc_slots(self):
        compiler=shutil.which("clang") or shutil.which("gcc")
        if not compiler:self.skipTest("C compiler absent")
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/"esp112_slots"
            cmd=[compiler,"-std=c11","-Wall","-Wextra","-Werror",
                 str(ROOT/"src/PlayerESP112Port/esp112_slots.c"),
                 str(ROOT/"tests/esp112_slots_harness.c"),"-o",str(exe)]
            subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("ESP112_STABLE_SLOTS: PASS",run.stdout)
    def test_guid_motion_smoothing_and_timer(self):
        compiler=shutil.which("clang") or shutil.which("gcc")
        if not compiler:self.skipTest("C compiler absent")
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/"esp112_motion"
            cmd=[compiler,"-std=c11","-Wall","-Wextra","-Werror",
                 str(ROOT/"src/PlayerESP112Port/esp112_motion.c"),
                 str(ROOT/"tests/esp112_motion_harness.c"),"-o",str(exe)]
            if sys.platform!="win32":cmd.append("-lm")
            subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("ESP112_ADAPTIVE_MOTION: PASS",run.stdout)
        source=(ROOT/"src/PlayerESP112Port/esp112_host335.c").read_text()
        self.assertIn("SetTimer(NULL,0u,ESP112_PROJECTION_INTERVAL_MS,NULL)",source)
        self.assertIn("KillTimer(NULL,g_refresh_timer)",source)
        self.assertIn("esp112_motion_step(&g_motion[slot],c->guid",source)
        self.assertIn('\\\\\\"probe\\\\\\":\\\\\\"cadence\\\\\\"',source)
        self.assertIn("g_debug_pairs && (unsigned)slot<ESP112_DIAG_PAIRS",source)
    def test_projection_tick_separate_from_scan_and_stable_slots(self):
        src=(ROOT/"src/PlayerESP112Port/esp112_host335.c").read_text()
        ui=(ROOT/"src/PlayerESP112Port/esp112_overlay.c").read_text()
        self.assertIn("ESP112_PROJECTION_INTERVAL_MS 16u",src)
        self.assertIn("ESP112_OBJECT_SCAN_INTERVAL_MS 50u",src)
        self.assertIn("esp112_slots_reserve(&g_slots,c->guid,visible_mask)",src)
        self.assertIn("esp112_overlay_finish_frame(&g_overlay,visible_mask)",src)
        self.assertNotIn("esp112_overlay_hide_unused(&g_overlay,drawn)",src)
        self.assertIn("SWP_NOZORDER",ui)
    def test_os_overlay_not_lua(self):
        g=(ROOT/"src/PlayerESP112Port/esp112_overlay.c").read_text()
        self.assertIn("CreateWindowExA",g)
        self.assertIn("WS_EX_LAYERED",g)
        self.assertIn("ClientToScreen", (ROOT/"src/PlayerESP112Port/esp112_host335.c").read_text())
        self.assertNotIn("UIParent:Get",g)
    def test_debug_pairs_bind_same_guid_and_ground_anchor(self):
        host=(ROOT/"src/PlayerESP112Port/esp112_host335.c").read_text()
        overlay=(ROOT/"src/PlayerESP112Port/esp112_overlay.c").read_text()
        self.assertIn("candidate.world_base=p->position;",host)
        self.assertIn("native_project(world_frame,c->world_base,&view,&foot)",host)
        self.assertIn("esp112_overlay_show_foot(&g_overlay,(unsigned)slot",host)
        self.assertIn('\\\"probe\\\":\\\"paired_head_and_feet\\\"',host)
        self.assertIn("ESP112_DIAG_PAIRS 6u",(ROOT/"src/PlayerESP112Port/esp112_overlay.h").read_text())
        self.assertIn("foot_window",overlay)
        self.assertIn("esp112_overlay_hide_foot(o,i);",overlay)
    def test_native_single_ddc_and_bottom_up_y(self):
        host=(ROOT/"src/PlayerESP112Port/esp112_host335.c").read_text()
        geom=(ROOT/"src/PlayerESP112Port/esp112_geometry.c").read_text()
        self.assertIn("esp112_ui_to_client(screen_xyz[0],screen_xyz[1],scale_x,scale_y",host)
        self.assertNotIn("ddc(screen_xyz[0],screen_xyz[1]",host)
        self.assertNotIn("NativeDdcToNdc ddc=",host)
        self.assertIn("y=uy/scaley;",geom)
        self.assertIn("(1.f-y)*(float)v->height",geom)
        self.assertNotIn("*out_y=(int)(y*(float)v->height",geom)
if __name__=="__main__":unittest.main()
