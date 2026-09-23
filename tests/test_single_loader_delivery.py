"""The canonical updater ships one manifest-driven runtime launcher only."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class SingleLoaderDeliveryTests(unittest.TestCase):
    def test_one_launcher_and_no_native_preview(self):
        workflow = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        dashboard = (ROOT / "tools/updater/Updater335Dashboard.cs").read_text(encoding="utf-8")
        runtime = (ROOT / "tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text(encoding="utf-8")
        self.assertFalse((ROOT / "tools/updater/UpdaterAutoLootNativePreviewFeature.cs").exists())
        self.assertNotIn("UpdaterAutoLootNativePreviewFeature.cs", workflow)
        self.assertNotIn("NativeAutoLootPreviewAsync", dashboard)
        self.assertNotIn("autoLootNativePreviewButton", dashboard)
        self.assertNotIn("Natywny AutoLoot TEST", dashboard)
        self.assertIn("src\\Loader\\loader_win32.c", workflow)
        self.assertIn("WoW335Runtime.Loader.exe", workflow)
        self.assertIn("NativeCheckX86(bytes, false)", runtime)
        self.assertIn("NativeRejectReparse(loader)", runtime)

    def test_legacy_epoch_cannot_launch_second_loader(self):
        guard = (ROOT / "tools/updater/UpdaterSafety.cs").read_text(encoding="utf-8")
        runtime = (ROOT / "tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text(encoding="utf-8")
        install = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        repair = (ROOT / "tools/updater/UpdaterMaintenanceFeature.cs").read_text(encoding="utf-8")
        self.assertIn('"Wow335Loader.dll"', guard)
        self.assertIn('"epoch_test"', guard)
        for src in (runtime, install, repair):
            self.assertIn("UpdaterSafety.RequireNoLegacyEpoch(root);", src)

if __name__ == "__main__":
    unittest.main()
