"""Renderer ownership/integration assertions; actual in-game alignment unverified."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class EspRenderIntegration(unittest.TestCase):
    def test_one_owner_and_camera_on_game_thread(self):
        r=(ROOT/"src/PlayerESP/player_esp_d3d9.c").read_text()
        h=(ROOT/"src/PlayerESP/player_esp_win32_host.c").read_text()
        w=(ROOT/".github/workflows/consolexp_reference_esp.yml").read_text()
        self.assertEqual(r.count("InterlockedCompareExchangePointer("),1)
        self.assertIn("IDirect3DStateBlock9_Apply(block)",r)
        self.assertIn("IDirect3DDevice9_DrawPrimitiveUP",r)
        self.assertIn("esp335_camera_build(&axes,&camera)",h)
        self.assertIn("esp335_d3d9_draw_labels(",h)
        self.assertIn("g_frame_snapshot=g_scanner.snapshot",h)
        self.assertIn("TryEnterCriticalSection(&g_frame_lock)",h)
        self.assertIn("read_camera_axes(&axes)",h)
        self.assertIn("esp335_d3d9_draw_panel(device,",h)
        self.assertIn("player_esp_d3d9.c",w)
        self.assertIn("d3d9.lib",w)
    def test_fails_closed_for_unknown_metadata(self):
        core=(ROOT/"src/PlayerESP/player_esp_core.c").read_text()
        self.assertIn("filter->show_all",core)
        self.assertIn("filter.show_all=(flags & ESP335_GUI_PLAYERS)", (ROOT/"src/PlayerESP/player_esp_win32_host.c").read_text())
if __name__=="__main__":unittest.main()
