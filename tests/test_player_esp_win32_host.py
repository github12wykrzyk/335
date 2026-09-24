import sys
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/"tools"))
from verify_esp_native import inspect

class EspHostTests(unittest.TestCase):
    def test_host_is_readonly_and_uses_existing_loader(self):
        source = (ROOT/"src/PlayerESP/player_esp_win32_host.c").read_text()
        for export in ("W335_MessageId", "W335_HookProc", "W335_CallWndProc"):
            self.assertIn(export, source)
        for forbidden in ("SetWindowsHookEx(", "VirtualProtect(", "MH_CreateHook(",
                          "WriteProcessMemory(", "TerminateProcess("):
            self.assertNotIn(forbidden, source)
        self.assertIn("esp335_scanner_collect", source)
        self.assertIn("ESP335_EXACT_EXE_SHA256", source)
        self.assertIn("retry; do not disable UI or message hooks", source)
        self.assertIn("render_attempts", source)
        self.assertIn("g_enabled=1u;\n    try_renderer();", source)
        self.assertIn("g_scanner.bound ? esp335_scanner_collect", source)
        self.assertIn("GetAsyncKeyState(VK_INSERT)", source)
        self.assertIn("poll_insert();", source)
        self.assertIn("g_key_toggled=1u;", source)
    def test_non_pe_cannot_be_a_game_dll(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake = Path(tmp)/"PlayerESP335.dll"
            fake.write_bytes(b"MZ" + bytes(512))
            with self.assertRaises(ValueError):
                inspect(fake, "W335_MessageId W335_HookProc W335_CallWndProc")

if __name__ == "__main__":
    unittest.main()
