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
        main_policy=POLICY[POLICY.index('static PpResult cast_result('):]
        self.assertLess(main_policy.index('!strcmp(empty,want)'),
                        main_policy.index('return PP_RESULT_SUCCESS;'))
        self.assertLess(main_policy.index('!strcmp(fail,want)'),
                        main_policy.index('return PP_RESULT_SUCCESS;'))
        self.assertIn('dg==_G.W335PP_G',POLICY)
        self.assertIn('t-_G.W335PP_T<=0.4',POLICY)
        self.assertIn('W335PP_RANGE',POLICY)
        self.assertIn('SPELL_FAILED_OUT_OF_RANGE',POLICY)
        self.assertIn('ERR_OUT_OF_RANGE',POLICY)
        self.assertIn('!strcmp(range,want)',POLICY)
    def test_wallet_signal_is_nonce_scoped_bounded_and_not_money_alone(self):
        core=(ROOT/"src/AutoPickPocket/autopickpocket_core.c").read_text()
        adapter=(ROOT/"src/AutoPickPocket/autopickpocket_12340_adapter.c").read_text()
        header=(ROOT/"src/AutoPickPocket/autopickpocket_core.h").read_text()
        self.assertIn("f:RegisterEvent('PLAYER_MONEY')",POLICY)
        self.assertIn("_G.W335PP_MB=GetMoney and GetMoney() or -1",POLICY)
        self.assertIn("t-_G.W335PP_T<=0.4",POLICY)
        self.assertIn("if balance>_G.W335PP_MB then",POLICY)
        self.assertIn("_G.W335PP_M=n",POLICY)
        self.assertIn("!strcmp(money,want) && !strcmp(loot,want)",POLICY)
        self.assertIn("(!source_read || (source.lo|source.hi)==0u || same(source,guid))",POLICY)
        self.assertLess(POLICY.index("!strcmp(empty,want)"),
                        POLICY.index("!strcmp(money,want) && !strcmp(loot,want)"))
        self.assertLess(POLICY.index("!strcmp(fail,want)"),
                        POLICY.index("!strcmp(money,want) && !strcmp(loot,want)"))
        self.assertIn("return PP_RESULT_MONEY_SUCCESS;",POLICY)
        self.assertIn("PP_RESULT_MONEY_SUCCESS = 5",header)
        self.assertIn("PP_EVENT_MONEY_SUCCESS = 17",header)
        self.assertIn("case PP_RESULT_MONEY_SUCCESS:",core)
        self.assertIn("emit(engine,PP_EVENT_MONEY_SUCCESS,engine->active);",core)
        self.assertIn("result>PP_RESULT_CAST_REJECTED",adapter)
        self.assertIn('reason="wallet_loot_signal"',HOST)
        self.assertIn("kind==PP_EVENT_MONEY_SUCCESS",HOST)
    def test_exact_attempt_cancellation_unifies_timers(self):
        core=(ROOT/"src/AutoPickPocket/autopickpocket_core.c").read_text()
        adapter=(ROOT/"src/AutoPickPocket/autopickpocket_12340_adapter.c").read_text()
        host_header=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.h").read_text()
        core_header=(ROOT/"src/AutoPickPocket/autopickpocket_core.h").read_text()
        self.assertIn("PP_RESULT_TIMEOUT_MS 400u",core_header)
        self.assertIn("pending_size()>=PP_MAX_PENDING",POLICY)
        self.assertIn("GetTickCount()-pending[idx].started",POLICY)
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
    def test_four_guid_burst_is_bounded_and_correlated(self):
        core=(ROOT/"src/AutoPickPocket/autopickpocket_core.c").read_text()
        header=(ROOT/"src/AutoPickPocket/autopickpocket_core.h").read_text()
        for token in ("PP_MAX_PENDING 4u","PP_BURST_MIN_CAST_GAP_MS 100u",
                      "PpInFlight pending[PP_MAX_PENDING]"):
            self.assertIn(token,header)
        for token in ("static void tick_burst(","static void poll_burst(",
                      "if(!engine->probe_mode){","is_pending(e,t.guid)",
                      "(uint32_t)(now-e->last_burst_cast_ms)<PP_BURST_MIN_CAST_GAP_MS",
                      "e->api.result(e->api.ctx,p->guid,p->attempt_id)",
                      "e->api.end_attempt(e->api.ctx,p->guid,p->attempt_id)",
                      "p->valid=0u"):
            self.assertIn(token,core)
        for token in ("PpGuid guid;uint32_t nonce,started;int valid;",
                      "pending_index(guid,nonce)","if(pending_size()>=PP_MAX_PENDING)",
                      "burst_overlap=1","if(burst_overlap || !active",
                      "return burst_result(guid,nonce,elapsed);",
                      "rec=_G.W335PP_BURST and _G.W335PP_BURST[dg]",
                      "_G.W335PP_BURST[_G.W335PP_G]={n=_G.W335PP_N",
                      "t-rec.t>1.5","string.upper(src)==string.upper(UnitGUID('player'))",
                      "rec.s='1'","rec.f=why=='U' and 'F' or why",
                      "_G.W335PP_BURST['0X%08lX%08lX']",
                      "r.n=='%lu'",
                      "if r.f~='0'","elseif r.s=='1'",
                      "p[g].n=='%lu' then p[g]=nil"):
            self.assertIn(token,POLICY)
        self.assertNotIn("ready_for_burst(",core)
        self.assertIn("engine->burst_mode=1u;",core)
        self.assertLess(POLICY.index("if(burst_overlap || !active"),
                        POLICY.index("!strcmp(money,want) && !strcmp(loot,want)"))
        adapter_source=(ROOT/"src/AutoPickPocket/autopickpocket_12340_adapter.c").read_text()
        adapter_header=(ROOT/"src/AutoPickPocket/autopickpocket_12340_adapter.h").read_text()
        self.assertIn("object_cache[PP_SCAN_CAP]",adapter_header)
        self.assertIn("a->object_cache_manager==manager",adapter_source)
        self.assertIn("guid_at(a,a->object_cache[n].obj,&cached)",adapter_source)
        self.assertIn("if(!target_obj)for(i=0u;i<PP_SCAN_LIMIT",adapter_source)
        self.assertIn("a->host.position(a->host.ctx,target_obj,target)",adapter_source)
        self.assertIn("pending_count",HOST)
        self.assertIn("g_timings[i].nonce==attempt_id",HOST)
        self.assertIn("result_wait=(uint32_t)(now-g_timings[i].started)",HOST)
        self.assertNotIn("GetTickCount()-started_ms)<1500u",POLICY)
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
            for forbidden in ("TargetUnit(", "on_success", "release_completed_target"):
                self.assertNotIn(forbidden,source)
        self.assertNotIn("ClearTarget(",HOST)
        self.assertIn("policy.after_cast_submitted=after_cast_submitted",POLICY)
        self.assertIn("g_policy.after_cast_submitted(g_policy.context,target,attempt_id)",HOST)
        self.assertIn("void (*after_cast_submitted)",(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.h").read_text())
        self.assertIn("nonce!=current_attempt",POLICY)
        self.assertIn("same(guid,current_target))return;",POLICY)
        self.assertIn("_G.W335PP_N=='%lu'",POLICY)
        self.assertIn("_G.W335PP_G=='0X%08lX%08lX'",POLICY)
        self.assertIn("string.upper(t)==_G.W335PP_G",POLICY)
        self.assertIn("if t and ClearTarget and string.upper(t)==_G.W335PP_G",POLICY)
        self.assertIn("then ClearTarget() end",POLICY)
        self.assertEqual(POLICY.count("ClearTarget()"),2)
        self.assertLess(HOST.index("((cast_fn)va)(spell,0u,target.lo,target.hi,0u);"),
                        HOST.index("g_policy.after_cast_submitted(g_policy.context,target,attempt_id)"))
        for metric in ("scan_ms","scan_candidates","queue_depth","queue_age_ms",
                       "pulse_gap_ms","cast_gap_ms","result_wait_ms","next_wait_ms"):
            self.assertIn(metric,HOST)
        self.assertIn("h.clock_ms=clock_ms",HOST)
if __name__=="__main__":
    unittest.main()
