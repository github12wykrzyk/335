"""Native WoW 12340 readiness, observer and exact GUID/nonce result guards."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
POLICY=(ROOT/"src/AutoPickPocket/autopickpocket_game_policies.c").read_text()
HOST=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
WF=(ROOT/".github/workflows/activate_autopickpocket.yml").read_text()
class SpellAndResultTests(unittest.TestCase):
    def test_readiness_checks_all_conditions(self):
        for value in ("IsSpellKnown(921)","IsStealthed()","IsUsableSpell(name)",
                      "UnitClass('player')","GetSpellCooldown(921)","W335PP_READY"):
            self.assertIn(value,POLICY)
    def test_crashed_lua_c_callback_never_registered(self):
        # WoW #134: Lua rejects a native DLL C callback in PP335_LootOpened.
        for unsafe in ('FrameScript_RegisterFunction','PP335_LootOpened',
                       'lua_loot_opened','register_loot_callback','LUA_REGISTER'):
            self.assertNotIn(unsafe,POLICY)
        self.assertIn("f:SetScript('OnEvent'",POLICY)
        self.assertIn("f:RegisterEvent('LOOT_OPENED')",POLICY)
        self.assertIn('_G.W335PP_O=n',POLICY)
    def test_nonce_guid_correlated_server_result_and_bounded_fallback(self):
        for value in ('SPELL_CAST_SUCCESS','COMBAT_LOG_EVENT_UNFILTERED',
                      'nonce!=current_attempt','!same(guid,current_target)',
                      'read_u32(LOOT_SOURCE,&source.lo)','same(source,guid)',
                      'PP_RESULT_PENDING','PP_RESULT_EMPTY',
                      'PP_RESULT_OUT_OF_RANGE','PP_RESULT_CAST_REJECTED','elapsed>=80u'):
            self.assertIn(value,POLICY)
        self.assertLess(POLICY.index('!strcmp(empty,want)'),
                        POLICY.index('return PP_RESULT_SUCCESS;'))
        self.assertLess(POLICY.index('!strcmp(fail,want)'),
                        POLICY.index('return PP_RESULT_SUCCESS;'))
        self.assertIn('dg==_G.W335PP_G',POLICY)
        self.assertIn('t-_G.W335PP_T<=0.4',POLICY)
        self.assertIn('W335PP_RANGE',POLICY)
        self.assertIn('SPELL_FAILED_OUT_OF_RANGE',POLICY)
        self.assertIn('ERR_OUT_OF_RANGE',POLICY)
        self.assertIn('!strcmp(range,want)',POLICY)
    def test_exact_attempt_cancellation_unifies_timers(self):
        core=(ROOT/"src/AutoPickPocket/autopickpocket_core.c").read_text()
        adapter=(ROOT/"src/AutoPickPocket/autopickpocket_12340_adapter.c").read_text()
        host_header=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.h").read_text()
        core_header=(ROOT/"src/AutoPickPocket/autopickpocket_core.h").read_text()
        self.assertIn("PP_RESULT_TIMEOUT_MS 900u",core_header)
        self.assertIn("GetTickCount()-started_ms)<PP_RESULT_TIMEOUT_MS",POLICY)
        self.assertIn("if(elapsed>PP_RESULT_TIMEOUT_MS)",POLICY)
        self.assertNotIn("GetTickCount()-started_ms)<1500u",POLICY)
        self.assertIn("nonce==current_attempt",POLICY)
        self.assertIn("same(guid,current_target))clear();",POLICY)
        self.assertIn("policy.end_attempt=end_attempt",POLICY)
        self.assertIn("g_policy.end_attempt(g_policy.context,guid,attempt_id)",HOST)
        self.assertIn("h.end_attempt=end_attempt",HOST)
        self.assertIn("api.end_attempt=pp_end_attempt",adapter)
        self.assertIn("a->host.end_attempt(a->host.ctx,guid,attempt_id)",adapter)
        self.assertIn("engine->api.end_attempt(engine->api.ctx,engine->active,engine->active_attempt_id)",core)
        self.assertIn("e->api.end_attempt(e->api.ctx,selected,e->active_attempt_id)",core)
        self.assertIn("void (*end_attempt)",host_header)
    def test_pinned_bridge_compiled_and_no_per_tick_reenable(self):
        self.assertIn("0x00819210u",POLICY)
        self.assertIn("0x00818010u",POLICY)
        self.assertIn("tools/register_autopickpocket.py",WF)
        self.assertIn("autopickpocket_game_policies.c", (ROOT/"tools/register_autopickpocket.py").read_text())
        self.assertIn("PP335_VerifiedPolicyV1",WF)
        self.assertIn("verify_hash(NULL,PP12340_CLIENT_SHA256)",HOST)
        self.assertIn("if (g_initialized && command==2u)",HOST)
        self.assertNotIn("LootSlot(",POLICY)
        for source in (POLICY,HOST):
            for forbidden in ("ClearTarget(", "TargetUnit(", "UnitGUID('target')",
                              "on_success", "release_completed_target"):
                self.assertNotIn(forbidden,source)
        for metric in ("scan_ms","scan_candidates","queue_depth","queue_age_ms",
                       "pulse_gap_ms","cast_gap_ms","result_wait_ms","next_wait_ms"):
            self.assertIn(metric,HOST)
        self.assertIn("h.clock_ms=clock_ms",HOST)
if __name__=="__main__":
    unittest.main()
