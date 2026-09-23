"""No message-based loot ticks; exact client, world vtable and thread are gated."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class FrameContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src=(ROOT/"src/AutoLoot/autoloot_win32_host.c").read_text(encoding="utf-8")
    def test_verified_slot_and_object(self):
        s=self.src
        for x in ["0x004FA040u","0x009F9A50u","0x00EEEA8Cu","AL12340_EXACT_EXE_SHA256",
                  "read32(NULL,AL_WORLD_FRAME_GLOBAL,&world)","read32(NULL,(uintptr_t)world,&vtable)",
                  "vtable>slot","slot-vtable>=512u",
                  "original!=(uint32_t)AL_WORLD_LAYER_ORIGINAL",
                  "equal_bytes(AL_WORLD_LAYER_ORIGINAL,expected,sizeof(expected))",
                  "InterlockedCompareExchangePointer","VirtualProtect",
                  "GetCurrentThreadId() == g_game_thread"]:
            self.assertIn(x,s)
    def test_frame_owns_all_engine_ticks(self):
        s=self.src
        self.assertEqual(s.count("al12340_tick(&g_engine,"),1)
        self.assertIn("static __declspec(noinline) void AL335_FrameTick",s)
        self.assertIn("call AL335_FrameTick",s)
        self.assertIn("jmp dword ptr [g_original_frame]",s)
        self.assertIn("pushfd",s)
        self.assertIn("pushad",s)
        self.assertIn("popad",s)
        self.assertIn("popfd",s)
        self.assertIn("if (!is_game_thread())",s)
        self.assertIn("if (g_driving) return;",s)
        self.assertIn("AL335_ArmFrame();",s)
        self.assertNotIn("drive_engine();",s)
    def test_nonrunnable_experiment(self):
        f=(ROOT/".github/workflows/build_autoloot_frame.yml").read_text()
        self.assertIn("FINAL_PACKAGE: NOT_RUN",f)
        self.assertIn("ISOLATED_FRAME_BINARY_NOT_GAME_PACKAGE",f)
        self.assertNotIn("update_ref",f)
        self.assertIn("Verify exact 12340 client",f)
if __name__=="__main__":
    unittest.main()
