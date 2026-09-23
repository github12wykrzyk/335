"""Shared loader source-level invariants; game behavior needs the exact-SHA TEST."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
LOADER=ROOT/"src/Loader12340/Wow335Loader.c"
HOST=ROOT/"src/AutoLoot/autoloot_win32_host.c"
WORKFLOW=ROOT/".github/workflows/build_shared_loader.yml"

class SharedLoaderTests(unittest.TestCase):
    def test_same_hook_owner_drives_both_modules(self):
        loader=LOADER.read_text(encoding="utf-8")
        host=HOST.read_text(encoding="utf-8")
        self.assertIn("AL335_ConfigurePeer",loader)
        self.assertIn("AL335_PeerStatus",loader)
        self.assertIn("PP335_BindOnGameThread",loader)
        self.assertIn("PP335_TickOnGameThread",loader)
        self.assertIn("PP335_EnableOnGameThread",loader)
        self.assertIn("AutoPickPocket335.dll",loader)
        self.assertIn("SHARED_EXPORTS_MISSING",loader)
        self.assertIn("POLICY_NOT_VERIFIED",loader)
        self.assertIn("GAME_THREAD_BIND_FAILED",loader)
        self.assertLess(loader.index("configure_pp_peer(host,pp,&peer_status)"),
                        loader.index("SetWindowsHookExW(WH_GETMESSAGE"))
        self.assertIn("SetWindowsHookExW(WH_GETMESSAGE,callback,host,game.tid)",loader)
        self.assertIn("SetWindowsHookExW(WH_CALLWNDPROC,dispatch,host,game.tid)",loader)
        self.assertEqual(loader.count("SetWindowsHookExW("),2)
        self.assertNotIn("SetWindowsHookExW",host)
        self.assertIn("g_peer.thread==GetCurrentThreadId()",host)
        self.assertIn("g_peer.tick((uint32_t)now)",host)
        self.assertIn("g_peer.enable(0)",host)

    def test_missing_native_policy_never_silently_enables_pp(self):
        loader=LOADER.read_text(encoding="utf-8")
        host=HOST.read_text(encoding="utf-8")
        self.assertIn("PP335_VerifiedPolicyV1",loader)
        self.assertIn("if (pp && !configure_pp_peer(host,pp,&peer_status)) return 1;",loader)
        self.assertIn("peer_status()!=2",loader)
        self.assertIn("if (g_peer.status==1 || g_peer.status==-1) return;",host)
        self.assertIn("if (g_peer.status==2) g_peer.enable(0);",host)
        self.assertNotIn("g_peer.enable(1)",host)

    def test_exact_x86_build_is_never_labeled_game_package(self):
        script=WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("SHARED_LOADER_NATIVE_X86_BUILD",script)
        self.assertIn("full_game_package':False",script)
        self.assertIn("inspect_dll(p)",script)
        self.assertIn("never",script.lower())
        self.assertNotIn("FINAL_PACKAGE: PASS",script)

if __name__=="__main__":
    unittest.main()
