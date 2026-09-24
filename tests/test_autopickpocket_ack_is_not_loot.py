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
    def test_no_native_fallback(self):
        p=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
        self.assertIn("((send_fn)PP335_SEND_VA)(&packet)",p)
        self.assertNotIn("packet_no_ack_native_fallback",p)
if __name__=="__main__":unittest.main()
