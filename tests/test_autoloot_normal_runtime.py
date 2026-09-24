"""Native registered AutoLoot stack — coherent TEST updater delivery contract."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RegisteredAutoLootTests(unittest.TestCase):
    def test_universal_game_loader_uses_ordered_local_manifest_and_hook_chain(self):
        src = (ROOT / "src/Loader/loader_win32.c").read_text(encoding="utf-8")
        self.assertIn("read_manifest(directory, modules, &count)", src)
        self.assertIn("MAX_MODULES 32", src)
        self.assertIn("valid_name(line)", src)
        self.assertIn("_wcsicmp(modules[i].name, modules[n].name)", src)
        self.assertIn("LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)", src)
        self.assertIn("W335_MessageId", src)
        self.assertIn("W335_HookProc", src)
        self.assertIn("W335_CallWndProc", src)
        self.assertIn('L"AutoLoot335.dll"', src)
        self.assertIn("SetWindowsHookExW(WH_CALLWNDPROC", src)
        self.assertIn("SetWindowsHookExW(WH_GETMESSAGE", src)
        self.assertIn("SMTO_ABORTIFHUNG | SMTO_BLOCK", src)
        self.assertIn("TerminateProcess(pi.hProcess, 1u)", src)
        self.assertNotIn("PostMessageW(", src)

    def test_updater_multi_module_launch_requires_all_installed_exact_hashes(self):
        code = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        feature = (ROOT / "tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        self.assertIn("TryLaunchInstalledModules(root, exe, state)", code)
        self.assertNotIn("TryLaunchInstalledAutoLoot(root, exe, state)", code)
        self.assertIn('state["managed_sha256"] = fileHashes;', code)
        self.assertIn("WoW335Runtime.Loader.exe", feature)
        self.assertIn("registered.Length == 0", feature)
        self.assertIn("expected.SetEquals(listed)", feature)
        self.assertIn("listed.Length != registered.Length", feature)
        self.assertIn("Sha256File(dll)", feature)
        self.assertIn("Sha256File(order)", feature)
        self.assertIn("NativeCheckX86(File.ReadAllBytes(dll), true)", feature)
        self.assertIn("UpdaterBuildInfo.PinnedClientSha256", feature)
        self.assertIn("src\\Loader\\loader_win32.c", workflow)
        self.assertIn("WoW335Runtime.Loader.exe", workflow)

    def test_runtime_never_promotes_unregistered_or_unaccepted_modules(self):
        runtime = json.loads((ROOT / "runtime/current.json").read_text(encoding="utf-8"))
        registry = json.loads((ROOT / "runtime/module_registry.json").read_text(encoding="utf-8"))
        if runtime["state"] == "empty":
            self.assertEqual(runtime["files"], [])
            self.assertEqual(registry["modules"], [])
            return
        # The loader may accept additional real modules, but every DLL must
        # have an ordered owner and an exact checked-in PE32 x86 binary.
        components = [row["component"] for row in runtime["files"]]
        owners = [row["component"] for row in registry["modules"]]
        self.assertEqual(components[:2], ["Client12340", "AutoLoot"])
        self.assertEqual(components[1:], owners)
        self.assertNotIn("WoW335AutoLootDiag", components)
        from tools.manifest_common import pe_machine
        for item in runtime["files"]:
            if item["kind"] == "dll":
                self.assertEqual(item["arch"], "x86")
                self.assertEqual(item["sha256"],
                                 __import__("hashlib").sha256(
                                     (ROOT / item["path"]).read_bytes()).hexdigest())
                self.assertEqual(pe_machine(ROOT / item["path"]), 0x014c)
        self.assertEqual(runtime["files"][1]["path"], "runtime/AutoLoot335.dll")
        self.assertEqual(runtime["files"][1]["sha256"],
                         __import__("hashlib").sha256(
                             (ROOT / "runtime/AutoLoot335.dll").read_bytes()).hexdigest())
        self.assertEqual(runtime["files"][1]["arch"], "x86")
        self.assertEqual(registry["modules"][0]["build"]["toolchain"], "msvc_x86")


if __name__ == "__main__":
    unittest.main()
