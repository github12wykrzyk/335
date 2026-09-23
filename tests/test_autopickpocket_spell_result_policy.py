"""Native WoW 12340 readiness, observer and exact GUID/nonce result guards."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
POLICY=(ROOT/"src/AutoPickPocket/autopickpocket_game_policies.c").read_text()
HOST=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
WF=(ROOT/".github/workflows/build_shared_loader.yml").read_text()
class SpellAndResultTests(unittest.TestCase):
    def test_readiness_checks_all_conditions(self):
        for value in ("IsSpellKnown(921)","IsStealthed()","IsUsableSpell(name)",
                      "UnitClass('player')","GetSpellCooldown(921)","W335PP_READY"):
            self.assertIn(value,POLICY)
    def test_results_require_nonce_server_and_exact_loot_guid(self):
        for value in ("SPELL_CAST_SUCCESS","COMBAT_LOG_EVENT_UNFILTERED",
                      "LOOT_OPENED","nonce!=current_attempt",
                      "!same(guid,current_target)","read_u32(LOOT_SOURCE,&source.lo)", "loot_attempt==nonce",
                      "same(captured_loot_guid,guid)",
                      "same(source,current_target)","PP_RESULT_PENDING","PP_RESULT_EMPTY",
                      "PP_RESULT_RETRYABLE"):
            self.assertIn(value,POLICY)
        self.assertLess(POLICY.index("loot_attempt==nonce"),
                        POLICY.index("return PP_RESULT_SUCCESS;"))
        self.assertIn("FrameScript_RegisterFunction",POLICY)
        self.assertIn("PP335_LootOpened",POLICY)
    def test_pinned_bridge_compiled_and_no_per_tick_reenable(self):
        self.assertIn("0x00819210u",POLICY)
        self.assertIn("0x00818010u",POLICY)
        self.assertIn("autopickpocket_game_policies.c /link",WF)
        self.assertIn("PP335_VerifiedPolicyV1",WF)
        self.assertIn("verify_hash(NULL,PP12340_CLIENT_SHA256)",HOST)
        self.assertIn("if (g_initialized && command==2u)",HOST)
        self.assertNotIn("LootSlot(",POLICY)
if __name__=="__main__":
    unittest.main()
