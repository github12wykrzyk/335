"""Canonical updater must not launch a second loader over the Epoch experiment."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
class EpochBridgeGuardTests(unittest.TestCase):
    def test_game_install_launch_and_repair_fail_closed(self):
        safety = (ROOT / "tools/updater/UpdaterSafety.cs").read_text(encoding="utf-8")
        runtime = (ROOT / "tools/updater/UpdaterAutoLootRuntimeFeature.cs").read_text(encoding="utf-8")
        updater = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        maintenance = (ROOT / "tools/updater/UpdaterMaintenanceFeature.cs").read_text(encoding="utf-8")
        self.assertIn('"epoch_test"', safety)
        self.assertIn('"installed.json"', safety)
        self.assertIn('"modules.lock"', safety)
        self.assertIn('"Wow335Loader.dll"', safety)
        self.assertIn("UpdaterSafety.RequireNoLegacyEpoch(root);", runtime)
        self.assertIn("UpdaterSafety.RequireNoLegacyEpoch(root);", updater)
        self.assertIn("UpdaterSafety.RequireNoLegacyEpoch(root);", maintenance)
