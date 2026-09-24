"""Shared GUI integration contract. These are static checks, not gameplay."""
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from stage_gui_candidate import gui_contract,prepare_registration
from verify_module_registry import validate

class SharedGuiContracts(unittest.TestCase):
    def test_no_second_d3d_owner_or_independent_loader(self):
        code=(ROOT/"src/SharedGUI/w335_gui_win32.c").read_text(encoding="utf-8")
        for item in ("load_managed_manifest","dlls.txt","GetModuleHandleA",
                     "W335GUI_Register","W335GUI_Unregister","VK_INSERT",
                     "LB_GETCOUNT","LB_GETTOPINDEX","LB_SETTOPINDEX",
                     "WM_SETREDRAW,FALSE","if(!changed)return"):
            self.assertIn(item,code)
        for item in ("CreateToolhelp32Snapshot","Module32First","Module32Next",
                     "TH32CS_SNAPMODULE","MH_CreateHook","SetWindowsHookEx",
                     "Direct3DCreate9","LoadLibraryEx"):
            self.assertNotIn(item,code)
        self.assertLess(code.index("if(!changed)return"),
                        code.index("LB_RESETCONTENT"))
        self.assertIn("g_managed[i]",code)
        self.assertIn("g_manifest_ok",code)
        self.assertEqual(gui_contract()["build"]["toolchain"],"msvc_x86")
    def test_esp_uses_shared_gui_without_unverified_hostility(self):
        esp=(ROOT/"src/PlayerESP112Port/esp112_host335.c").read_text(encoding="utf-8")
        for item in ("W335GUI_Register","gui_unregister();","g_max_range",
                     "g_show_players","g_show_npcs","game_window_foreground"):
            self.assertIn(item,esp)
        self.assertNotIn('{"hostile","',esp)
        self.assertNotIn('{"mixed_bg","',esp)
    def test_full_runtime_registration_order_and_resource_owner(self):
        rt={"target":{"build":12340},"files":[
            {"component":"Client12340","kind":"exe"},
            {"component":"AutoLoot","kind":"dll","depends_on":[]},
            {"component":"PlayerESP","kind":"dll","depends_on":[]}],
            "compatibility_sets":[]}
        def module(name):
            return {"component":name,"sources":["src/README.md"],"requires":[],
              "resources":[
                {"id":"win32:WH_GETMESSAGE","mode":"chain","arbitrator":"Loader"},
                {"id":"win32:WH_CALLWNDPROC","mode":"chain","arbitrator":"Loader"}],
              "build":{"toolchain":"msvc_x86","sources":["src/README.md"],
                       "include_dirs":[],"libraries":[],"cflags":[],"ldflags":[]}}
        registry={"schema_version":1,"target_build":12340,
                  "modules":[module("AutoLoot"),module("PlayerESP")]}
        index={"modules":[{"component":"AutoLoot"},{"component":"PlayerESP"}]}
        new_rt,new_rg,new_idx=prepare_registration(rt,registry,index,
            {"AutoLoot":"a"*64,"SharedGUI":"b"*64,"PlayerESP":"c"*64})
        self.assertEqual([m["component"] for m in new_rt["files"]],
            ["Client12340","AutoLoot","SharedGUI","PlayerESP"])
        self.assertEqual([m["component"] for m in new_idx["modules"]],
            ["AutoLoot","SharedGUI","PlayerESP"])
        self.assertEqual(new_rg["modules"][-1]["requires"],["SharedGUI"])
        self.assertEqual(validate(new_rt,new_rg),[])
        newer_rt,newer_rg,newer_idx=prepare_registration(
            new_rt,new_rg,new_idx,
            {"AutoLoot":"a"*64,"SharedGUI":"d"*64,"PlayerESP":"c"*64})
        self.assertEqual([m["component"] for m in newer_rt["files"]],
            ["Client12340","AutoLoot","SharedGUI","PlayerESP"])
        self.assertEqual(newer_rt["files"][2]["sha256"],"d"*64)
        self.assertEqual(len(newer_rg["modules"]),3)
        self.assertEqual(len(newer_idx["modules"]),3)
        self.assertEqual(validate(newer_rt,newer_rg),[])

if __name__=="__main__":unittest.main()
