"""Universal WoW 335 launcher only accepts the exact updater-managed module stack."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class UniversalLoaderContractTests(unittest.TestCase):
    def test_manifest_gates_and_abi(self):
        launcher = (ROOT / "src/Loader/loader_win32.c").read_text(encoding="utf-8")
        updater = (ROOT / "tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text(encoding="utf-8")
        self.assertIn("MAX_MODULES 32", launcher)
        self.assertIn("!valid_name(line)", launcher)
        self.assertIn("n == 0", launcher)
        self.assertIn("modules[i].name, modules[n].name", launcher)
        self.assertIn('"_W335_HookProc@12"', launcher)
        self.assertIn('"_W335_CallWndProc@12"', launcher)
        self.assertIn("LOAD_WITH_ALTERED_SEARCH_PATH", launcher)
        self.assertIn("TerminateProcess(pi.hProcess, 1u)", launcher)
        self.assertIn("CallNextHookEx", (ROOT / "src/AutoLoot/autoloot_win32_host.c").read_text(encoding="utf-8"))
        self.assertIn("!expected.SetEquals(listed)", updater)
        self.assertIn("listed.Length != registered.Length", updater)
        self.assertIn("Sha256File(order)", updater)
        self.assertIn("Sha256File(dll)", updater)
        self.assertIn("NativeCheckX86(File.ReadAllBytes(dll), true)", updater)
        self.assertNotIn("listed.Length != 1", updater)

    def test_updater_is_independent_of_single_dll_name(self):
        updater = (ROOT / "tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text(encoding="utf-8")
        ci = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        self.assertNotIn("RegisteredAutoLootDll", updater)
        self.assertNotIn("RegisteredAutoLootLoader", updater)
        self.assertIn("TryLaunchInstalledModules", updater)
        self.assertIn("WoW335RuntimeLoader.exe", ci)
        self.assertIn("WoW335Runtime.Loader.exe", ci)
        self.assertNotIn("/DAL_NORMAL_RUNTIME", ci)


if __name__ == "__main__":
    unittest.main()
