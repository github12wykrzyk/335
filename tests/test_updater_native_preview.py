"""Native WoW 12340 AutoLoot preview is always an explicit, exact-SHA
updater-managed test; no fake FINAL_PACKAGE: PASS or implicit game activation.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class NativeAutoLootDeliveryTests(unittest.TestCase):
    def test_native_preview_explicit_only_and_fail_closed(self):
        source = (ROOT/"tools/updater/UpdaterAutoLootNativePreviewFeature.cs").read_text(encoding="utf-8")
        workflow = (ROOT/".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        dashboard = (ROOT/"tools/updater/Updater335Dashboard.cs").read_text(encoding="utf-8")
        self.assertIn('NativeBranch = "feature/autoloot-12340"', source)
        self.assertIn('UpdaterSafety.RequireLatestSuccessfulRun(', source)
        self.assertIn('GetString(run, "head_sha"), sha', source)
        self.assertIn('NATIVE_AUTOLOOT_PREVIEW_NOT_GAME_PACKAGE', source)
        self.assertIn('Sha256File(wow)', source)
        self.assertIn('Sha256(hostBytes)', source)
        self.assertIn('Sha256(launcherBytes)', source)
        self.assertIn('NativeCheckX86(bytes, name == NativeHostFile)', source)
        self.assertIn('native_preview', source)
        self.assertIn('DialogResult.Yes', source)
        self.assertIn('IsGameRunning(root)', source)
        self.assertNotIn('FINAL_PACKAGE: PASS', source)
        self.assertIn('Natywny AutoLoot TEST', dashboard)
        self.assertIn('UpdaterAutoLootNativePreviewFeature.cs', workflow)

if __name__ == "__main__":
    unittest.main()
