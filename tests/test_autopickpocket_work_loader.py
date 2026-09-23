"""Guard the actual work launcher ABI and fail-closed PP type selection."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
HOST=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text(encoding="utf-8")
HEADER=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.h").read_text(encoding="utf-8")
LOADER=(ROOT/"src/Loader/loader_win32.c").read_text(encoding="utf-8") if (ROOT/"src/Loader/loader_win32.c").is_file() else ""

class WorkLoaderBridgeTests(unittest.TestCase):
    def test_current_launcher_abi(self):
        for symbol in ("W335_MessageId", "W335_HookProc", "W335_CallWndProc"):
            self.assertIn(symbol, HOST)
            self.assertIn(symbol, HEADER)
            if LOADER:
                self.assertIn(symbol, LOADER)
        self.assertIn("CallNextHookEx(NULL,code,w,l)", HOST)
        self.assertIn("current_thread_owns_game_window()", HOST)

    def test_only_two_authorized_creature_types(self):
        self.assertIn("PP_CREATURE_UNDEAD 6u", HOST)
        self.assertIn("PP_CREATURE_HUMANOID 7u", HOST)
        self.assertIn("type!=PP_CREATURE_UNDEAD && type!=PP_CREATURE_HUMANOID", HOST)
        self.assertIn("type=native_creature_type(obj)", HOST)
        self.assertIn("verify_creature_type_abi()", HOST)
        self.assertIn("g_policy.creature_type(g_policy.context,obj,guid)!=type", HOST)
        self.assertNotIn("!policy->creature_type", HOST)

    def test_missing_policy_prevents_loader_activation(self):
        self.assertIn('GetProcAddress(g_self,', HOST)
        self.assertIn('"PP335_VerifiedPolicyV1"', HOST)
        self.assertIn("if (!verified_policy()) return 0u;", HOST)
        self.assertNotIn("PP335_VerifiedPolicyV1(void)", HOST)
        self.assertIn("PP335_BindOnGameThread(policy)", HOST)
        self.assertNotIn("LoadLibraryExW", HOST)

if __name__=="__main__":
    unittest.main()
