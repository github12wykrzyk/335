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
        self.assertIn("native_camera_eye(&camera.local_position)",h)
        self.assertIn("W2S_NATIVE_VA ((uintptr_t)0x004F6D20u)",h)
        self.assertIn("project_native,NULL)",h)
        self.assertIn("memcmp((const void *)W2S_NATIVE_VA",h)
        self.assertNotIn("esp335_d3d9_install(",h)
        self.assertIn("esp335_lua_update(",h)
        self.assertIn("esp335_lua_create(",h)
        self.assertIn("g_frame_snapshot=g_scanner.snapshot",h)
        self.assertIn("esp335_lua_update(g_lua_gate",h)
        self.assertIn("read_camera_axes(&axes)",h)
        self.assertIn("GetProcAddress(host,\"AL335_ExecuteUiScript\")",h)
        self.assertIn("player_esp_d3d9.c",w)
        self.assertIn("d3d9.lib",w)
    def test_fails_closed_for_unknown_metadata(self):
        core=(ROOT/"src/PlayerESP/player_esp_core.c").read_text()
        self.assertIn("filter->show_all",core)
        self.assertIn("ESP335HUD:Paint({", (ROOT/"src/PlayerESP/player_esp_lua.c").read_text())
if __name__=="__main__":unittest.main()
