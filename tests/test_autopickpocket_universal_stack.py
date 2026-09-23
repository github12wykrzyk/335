"""One universal updater/loader, exact branch manifest, no second PP launcher."""
import hashlib
import json
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class UniversalPPStack(unittest.TestCase):
    def test_pp_implements_universal_loader_abi(self):
        host=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
        loader=(ROOT/"src/Loader/loader_win32.c").read_text()
        updater=(ROOT/"tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text()
        for exported in ("W335_MessageId","W335_HookProc","W335_CallWndProc"):
            self.assertIn(exported,host)
            self.assertIn(exported,loader)
        self.assertIn("PP335_VerifiedPolicyV1",host)
        self.assertIn("PP335_BindOnGameThread(policy)",host)
        self.assertIn("current_thread_owns_game_window()",host)
        self.assertIn("read_manifest(directory, modules, &count)",loader)
        self.assertIn("expected.SetEquals(listed)",updater)
        self.assertIn("System.Diagnostics.Process.Start(start)",updater)
        self.assertFalse((ROOT/"src/Loader12340/Wow335Loader.c").exists())

    def test_packet_sender_signature_is_real_imm8_instruction(self):
        # A wrong imm32 CMP silently denied bind on every game-thread pulse.
        host=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
        audit=(ROOT/"tools/audit_pp_packet_12340.py").read_text()
        self.assertIn("0xF1,0x83,0xBE",host)
        self.assertIn("0x34,0x05,0x00,0x00,0x05",host)
        self.assertNotIn("0xF1,0x81,0xBE",host)
        self.assertIn('0x00632B50,"native_head","55 8b ec 56 8b f1 83 be 34 05 00 00 05"',audit)

    def test_no_ack_packet_transport_has_one_way_correlated_native_fallback(self):
        host=(ROOT/"src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
        policy=(ROOT/"src/AutoPickPocket/autopickpocket_game_policies.c").read_text()
        self.assertIn("g_packet_nonce==attempt_id",host)
        self.assertIn("PP_RESULT_TIMEOUT_MS-80u",host)
        self.assertIn("packet_no_ack_native_fallback",host)
        self.assertIn('g_last_submitted_native ? "native_fallback" : "packet"',host)
        self.assertIn("PP_EVENT_PACKET_FALLBACK",host)
        self.assertIn("PP_RESULT_MONEY_SUCCESS",policy)
        self.assertIn("PLAYER_MONEY",policy)

    def test_registered_pp_is_complete_or_stays_inactive(self):
        runtime=json.loads((ROOT/"runtime/current.json").read_text())
        registry=json.loads((ROOT/"runtime/module_registry.json").read_text())
        names=[x["component"] for x in runtime["files"]]
        owners=[x["component"] for x in registry["modules"]]
        self.assertEqual(names[:2],["Client12340","AutoLoot"])
        self.assertEqual(owners[:1],["AutoLoot"])
        if "AutoPickPocket" not in names:
            self.assertEqual(names,["Client12340","AutoLoot"])
            self.assertNotIn("AutoPickPocket",owners)
            return
        self.assertEqual(names,["Client12340","AutoLoot","AutoPickPocket"])
        self.assertEqual(owners,["AutoLoot","AutoPickPocket"])
        pp=runtime["files"][2]
        binary=ROOT/pp["path"]
        self.assertTrue(binary.is_file())
        self.assertEqual(hashlib.sha256(binary.read_bytes()).hexdigest(),pp["sha256"])
        self.assertEqual(pp["arch"],"x86")
        self.assertEqual(pp["kind"],"dll")
        hooks=("win32:WH_GETMESSAGE","win32:WH_CALLWNDPROC")
        for key in hooks:
            for m in registry["modules"]:
                resource=next(x for x in m["resources"] if x["id"]==key)
                self.assertEqual(resource["mode"],"chain")
                self.assertEqual(resource["arbitrator"],"AutoLoot")

if __name__=="__main__":unittest.main()
