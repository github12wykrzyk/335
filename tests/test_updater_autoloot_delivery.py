"""Regression contract for updater-only AutoLoot diagnostic promotion to work."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AutoLootUpdaterDeliveryTests(unittest.TestCase):
    def test_embedded_addon_and_updater_delivery_path(self):
        workflow = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        maintenance = (ROOT / "tools/updater/UpdaterMaintenanceFeature.cs").read_text(encoding="utf-8")
        dashboard = (ROOT / "tools/updater/Updater335Dashboard.cs").read_text(encoding="utf-8")
        source = (ROOT / "tools/updater/UpdaterAutoLootDiagFeature.cs").read_text(encoding="utf-8")
        report = (ROOT / "tools/updater/UpdaterIssueReportFeature.cs").read_text(encoding="utf-8")
        version = (ROOT / "tools/updater/UpdaterSafety.cs").read_text(encoding="utf-8")
        self.assertIn('Version = "0.3.8-335-epoch-test"', version)
        self.assertIn('UpdaterArtifactPrefix + liveHead', maintenance)
        self.assertIn('GetString(meta, "git_sha"), liveHead', maintenance)
        self.assertIn('GetString(meta, "channel"), branch', maintenance)
        self.assertIn('TakeFeatureButton("WYŚLIJ LOG AUTOLOOT")', dashboard)
        self.assertIn('Instaluj test AutoLoot', dashboard)
        self.assertIn('Natywny AutoLoot TEST', dashboard)
        self.assertIn('SendReportAsync(true)', report)
        self.assertIn('ConfirmAutoLootReport', report)
        self.assertIn('AutoLootDiagSupport.CollectLog(root)', report)
        self.assertIn('AutoLootDiagSupport.SanitizeLog(', report)
        self.assertIn('AutoLootDiag.SourceCommit.txt', workflow)
        self.assertIn('tools\\updater\\tests\\AutoLootDiagSmoke.ps1', workflow)
        self.assertIn('PinnedClientSha256', source)
        self.assertIn('RejectLink(', source)
        self.assertIn('autoloot_diag_backups', source)
        self.assertIn('AUTOLOOT_DIAG_MANAGED_V1', source)
        self.assertIn('CollectLogFile(picker.FileName)', report)
        self.assertIn('ExplainMissingLog(root)', report)
        self.assertIn('onlyAutoLoot', report)
        self.assertIn('Issues: Read and write', report)
        for name in ('WoW335AutoLootDiag.lua', 'WoW335AutoLootDiag.toc'):
            self.assertTrue((ROOT / 'src/AutoLoot/diagnostics/WoW335AutoLootDiag' / name).is_file())

    def test_diagnostic_not_registered_as_game_dll(self):
        runtime = json.loads((ROOT / "runtime/current.json").read_text(encoding="utf-8"))
        registry = json.loads((ROOT / "runtime/module_registry.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime["state"], "empty")
        self.assertEqual(runtime["files"], [])
        self.assertEqual(registry["modules"], [])


if __name__ == "__main__":
    unittest.main()
