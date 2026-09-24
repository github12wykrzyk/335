"""A native UI fallback must not rely on a vtable from an inert D3D9 device."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class EspLuaFallback(unittest.TestCase):
    def test_ui_builds_real_wow_frames_and_markers(self):
        src=(ROOT/"src/PlayerESP/player_esp_lua.c").read_text()
        assert 'CreateFrame(\\"Frame\\",nil,UIParent)' in src
        assert 'CreateFrame(\\"Button\\",nil,frame)' in src
        assert 'ESP335HUD:SetVisible' in src
        assert 'ESP335HUD:Paint({' in src
        assert 'esp335_project(cam,p->position' in src
        assert 'ESP335_LUA_POINTS 48u' in src
    def test_only_autoloot_executes_client_lua(self):
        loot=(ROOT/"src/AutoLoot/autoloot_win32_host.c").read_text()
        esp=(ROOT/"src/PlayerESP/player_esp_win32_host.c").read_text()
        self.assertIn("AL335_ExecuteUiScript",loot)
        self.assertIn("AL12340_LUA_EXECUTE_VA",loot)
        self.assertNotIn("0x00819210",esp)
if __name__=="__main__": unittest.main()
