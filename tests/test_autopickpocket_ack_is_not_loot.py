"""Regression: a GUID-scoped cast ACK cannot be counted as completed theft."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ResultSeparation(unittest.TestCase):
    def test_live_policy_has_no_cast_only_success(self):
        p=(ROOT/"src/AutoPickPocket/autopickpocket_game_policies.c").read_text()
        self.assertIn("return PP_RESULT_CAST_ACK;",p)
        self.assertNotIn("return PP_RESULT_SUCCESS;",p)
        self.assertIn("same(source,guid)",p)
        self.assertLess(p.index("return PP_RESULT_MONEY_SUCCESS;"),p.index("if(q_success[0]=='1' &&"))
    def test_ack_keeps_burst_pending(self):
        p=(ROOT/"src/AutoPickPocket/autopickpocket_core.c").read_text()
        self.assertIn("if(out==PP_RESULT_CAST_ACK)",p)
        self.assertIn("out=PP_RESULT_PENDING;",p)
        self.assertIn("if(!p->ack_seen)",p)
        self.assertIn("if(p->ack_seen)",p)
        self.assertIn("PP_EVENT_ACK_LOOT_UNKNOWN",p)
        self.assertIn("block(e,p->guid,now,0u,1);",p)
    def test_spoof_position_held_then_bounded_restored_without_loot_claim(self):
        h=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
        p=(ROOT/"src/AutoPickPocket/autopickpocket_game_policies.c").read_text()
        for marker in ("PP335_SPOOF_HOLD_MAX_MS 750u",
                       "g_spoof_hold.active=1u",
                       "if(!enable)spoof_restore",
                       "spoof_service(now_ms)",
                       "spoof_restore(PP_EVENT_SPOOF_RESTORE_TIMEOUT)",
                       "if(!cast_sent)",
                       "spoof_restore(PP_EVENT_SPOOF_RESTORE_LOOT)",
                       "spoof_restore_send_failed_disable_spoof"):
            self.assertIn(marker,h)
        self.assertIn("!g_spoof_hold.active",h)
        self.assertIn("g_spoof_hold.world=g_adapter.current_world",h)
        self.assertIn("live_world=g_policy.world_token",h)
        self.assertIn("saved.world!=live_world",h)
        self.assertIn("current_lo!=saved.player.lo",h)
        self.assertIn("current_hi!=saved.player.hi",h)
        self.assertIn("position(NULL,player_obj,actual)",h)
        self.assertIn("send_heartbeat(saved.player,actual,actual_facing,",h)
        self.assertIn("PP_EVENT_SPOOF_WORLD_DROPPED",h)
        self.assertIn("spoof_transaction_done",p)
        self.assertIn("f:RegisterEvent('LOOT_CLOSED')",p)
        self.assertIn("_G.W335PP_C=n",p)
        self.assertNotIn("return PP_RESULT_MONEY_SUCCESS;",h)
        self.assertNotIn("send_heartbeat(player,me,facing,started+1u)",h)
    def test_no_native_fallback(self):
        p=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
        self.assertIn("((send_fn)PP335_SEND_VA)(&packet)",p)
        self.assertNotIn("packet_no_ack_native_fallback",p)
if __name__=="__main__":unittest.main()
