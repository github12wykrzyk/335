"""AutoLoot native message-delivery regression for held keys/mouse capture.

This is a source-level routing contract. Actual gameplay still requires a
player test for the exact GitHub Actions SHA on Windows WoW 12340 x86.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AutoLootMessageTransportTests(unittest.TestCase):
    def test_launcher_routes_commands_to_game_window_instead_of_thread_only(self):
        s = (ROOT / "src/AutoLoot/autoloot_win32_launcher.c").read_text(encoding="utf-8")
        self.assertIn("HWND hwnd;", s)
        self.assertIn("s->hwnd=hwnd;", s)
        self.assertIn("PostMessageW(s.hwnd,msg,1u,0u)", s)
        self.assertIn("PostMessageW(s.hwnd,msg,2u,0u)", s)
        self.assertIn("PostMessageW(s.hwnd,msg,0u,0u)", s)
        self.assertNotIn("PostThreadMessageW(", s)
        self.assertIn("WH_GETMESSAGE", s)
        self.assertIn("IsWindow(s.hwnd)", s)

    def test_game_thread_input_messages_drive_autoloot_with_bound_and_time_gates(self):
        s = (ROOT / "src/AutoLoot/autoloot_win32_host.c").read_text(encoding="utf-8")
        self.assertIn("if (msg->message == g_message)", s)
        self.assertIn("g_engine.bound && is_game_thread() && g_engine.engine.enabled", s)
        self.assertIn("msg->message != WM_QUIT", s)
        self.assertIn("(uint32_t)(now - g_last_drive_ms) >= 80u", s)
        self.assertIn("!g_driving", s)
        self.assertIn("al12340_tick(&g_engine, (uint32_t)now)", s)
        self.assertNotIn("if (msg->message != g_message) return", s)

if __name__ == "__main__":
    unittest.main()
