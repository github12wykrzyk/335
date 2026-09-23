"""Native registered AutoLoot stack — coherent TEST updater delivery contract."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RegisteredAutoLootTests(unittest.TestCase):
    def test_game_launcher_loads_only_selected_directory_registered_dll(self):
        src = (ROOT / "src/AutoLoot/autoloot_win32_launcher.c").read_text(encoding="utf-8")
        self.assertIn("#ifdef AL_NORMAL_RUNTIME", src)
        self.assertIn('L"AutoLoot335.dll"', src)
        self.assertIn("gameDir, AL_HOST_FILENAME", src)
        self.assertIn("SendMessageTimeoutW(hwnd, msg, command", src)
        self.assertIn("SMTO_ABORTIFHUNG | SMTO_BLOCK", src)
        self.assertIn("SetWindowsHookExW(WH_CALLWNDPROC", src)
        self.assertIn("send_control(s.hwnd,msg,2u,200u)", src)
        self.assertNotIn("PostMessageW(s.hwnd,msg,2u,0u)", src)
        self.assertNotIn("PostThreadMessageW(", src)

    def test_updater_auto_load_requires_installed_exact_hashes(self):
        code = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        feature = (ROOT / "tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        self.assertIn("TryLaunchInstalledAutoLoot(root, exe, state)", code)
        self.assertIn('state["managed_sha256"] = fileHashes;', code)
        self.assertIn("AutoLootRuntime.Launcher.exe", feature)
        self.assertIn("Sha256File(dll)", feature)
        self.assertIn("Sha256File(order)", feature)
        self.assertIn("UpdaterBuildInfo.PinnedClientSha256", feature)
        self.assertIn("RegisteredAutoLootDll", feature)
        self.assertIn("/DAL_NORMAL_RUNTIME", workflow)
        self.assertIn("AutoLootRuntime.Launcher.exe", workflow)

    def test_runtime_never_promotes_unregistered_or_unaccepted_modules(self):
        runtime = json.loads((ROOT / "runtime/current.json").read_text(encoding="utf-8"))
        registry = json.loads((ROOT / "runtime/module_registry.json").read_text(encoding="utf-8"))
        if runtime["state"] == "empty":
            self.assertEqual(runtime["files"], [])
            self.assertEqual(registry["modules"], [])
            return
        self.assertEqual([r["component"] for r in runtime["files"]],
                         ["Client12340", "AutoLoot"])
        self.assertEqual([r["component"] for r in registry["modules"]], ["AutoLoot"])
        self.assertEqual(runtime["files"][1]["path"], "runtime/AutoLoot335.dll")
        self.assertEqual(runtime["files"][1]["sha256"],
                         __import__("hashlib").sha256(
                             (ROOT / "runtime/AutoLoot335.dll").read_bytes()).hexdigest())
        self.assertEqual(runtime["files"][1]["arch"], "x86")
        self.assertEqual(registry["modules"][0]["build"]["toolchain"], "msvc_x86")


if __name__ == "__main__":
    unittest.main()
