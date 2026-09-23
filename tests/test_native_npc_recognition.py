"""Native creature type classifier fails closed; exact-client audit on Windows CI."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
HOST=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text(encoding="utf-8")
AUDIT=(ROOT/"src/AutoPickPocket/audit_npc_type_12340.py").read_text(encoding="utf-8")
BUILD=(ROOT/".github/workflows/build_shared_loader.yml").read_text(encoding="utf-8")

class NativeNpcRecognitionTests(unittest.TestCase):
    def test_correct_function_and_gamethread_only(self):
        self.assertIn("0x0071F300u",HOST)
        self.assertIn("mov ecx,obj",HOST)
        self.assertIn("call fn",HOST)
        self.assertIn("mov type,eax",HOST)
        self.assertIn("!is_game_thread()",HOST)
        self.assertIn("valid_memory((const void *)obj,0x9F8u)",HOST)
        self.assertIn("EXCEPTION_EXECUTE_HANDLER",HOST)
    def test_abi_and_rejection(self):
        self.assertIn("verify_creature_type_abi()",HOST)
        self.assertIn("PP_CREATURE_UNDEAD 6u",HOST)
        self.assertIn("PP_CREATURE_HUMANOID 7u",HOST)
        self.assertIn("type!=PP_CREATURE_UNDEAD && type!=PP_CREATURE_HUMANOID",HOST)
        self.assertIn("g_policy.creature_type(g_policy.context,obj,guid)!=type",HOST)
        self.assertIn("byte_match((uintptr_t)0x004F7494u,caller,sizeof(caller))",HOST)
        self.assertIn("va_bytes(TYPE_VA,len(TYPE_PREFIX))",AUDIT)
        self.assertIn("sha256(binary)",AUDIT)
        self.assertIn("python src/AutoPickPocket/audit_npc_type_12340.py",BUILD)
    def test_classification_is_not_mistaken_for_complete_pickpocket(self):
        self.assertIn("g_policy.eligible_npc(g_policy.context,obj,guid)==1",HOST)
        self.assertIn('if (!verified_policy()) return 0u;',HOST)
        self.assertNotIn("PP335_VerifiedPolicyV1(void)",HOST)
if __name__=="__main__":
    unittest.main()
