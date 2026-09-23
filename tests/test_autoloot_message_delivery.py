"""Source-level routing regression; in-game held-input behavior needs an exact-SHA test."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class AutoLootMessageTransportTests(unittest.TestCase):
    def test_launcher_sends_bounded_pulses_to_game_window(self):
        s = (ROOT / "src/AutoLoot/autoloot_win32_launcher.c").read_text(encoding="utf-8")
        self.assertIn("WH_GETMESSAGE", s)
        self.assertIn("WH_CALLWNDPROC", s)
        self.assertIn('GetProcAddress(host, "AL335_CallWndProc")', s)
        self.assertIn("SendMessageTimeoutW(hwnd, msg, command", s)
        self.assertIn("SMTO_ABORTIFHUNG | SMTO_BLOCK", s)
        self.assertIn("send_control(s.hwnd,msg,1u,2000u)", s)
        self.assertIn("send_control(s.hwnd,msg,2u,200u)", s)
        self.assertIn("Sleep(40)", s)
        self.assertNotIn("PostMessageW(s.hwnd,msg,2u,0u)", s)
        self.assertIn("UnhookWindowsHookEx(hook_dispatch)", s)
        self.assertIn("UnhookWindowsHookEx(hook_msg)", s)

    def test_host_uses_both_hooks_with_one_guarded_game_thread_engine(self):
        s = (ROOT / "src/AutoLoot/autoloot_win32_host.c").read_text(encoding="utf-8")
        self.assertIn("AL335_HookProc(int code", s)
        self.assertIn("AL335_CallWndProc(int code", s)
        self.assertIn("const CWPSTRUCT *msg", s)
        self.assertIn("handle_control(msg->message, msg->wParam)", s)
        self.assertEqual(s.count("drive_engine();"), 2)
        self.assertIn("g_engine.bound || !is_game_thread()", s)
        self.assertIn("g_driving=1u;", s)
        self.assertIn("if ((uint32_t)(now - g_last_drive_ms) < 40u) return;", s)
        self.assertIn("al12340_tick(&g_engine, (uint32_t)now)", s)

    def test_no_long_per_corpse_timeout_after_failed_open(self):
        s = (ROOT / "src/AutoLoot/autoloot_core.c").read_text(encoding="utf-8")
        self.assertIn("#define AL_FAILURE_COOLDOWN_MS 200u", s)
        self.assertIn("#define AL_SCAN_INTERVAL_MS 40u", s)
        self.assertIn("#define AL_OPEN_TIMEOUT_MS 600u", s)

if __name__ == "__main__":
    unittest.main()
