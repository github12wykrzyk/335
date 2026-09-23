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
