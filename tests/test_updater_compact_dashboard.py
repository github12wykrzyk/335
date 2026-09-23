"""Compact dashboard keeps exactly three primary actions and functional tools."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class CompactUpdaterDashboardTests(unittest.TestCase):
    def test_primary_actions_and_advanced_controls(self):
        ui = (ROOT / "tools/updater/Updater335Dashboard.cs").read_text(encoding="utf-8")
        self.assertIn('UiGrid(3, 1);\n            actions.Controls.Add(UiButton(checkButton, "Sprawdź")', ui)
        self.assertIn('UiButton(updateButton, "Aktualizuj")', ui)
        self.assertIn('UiButton(updatePlayButton, "Aktualizuj i uruchom", true)', ui)
        self.assertNotIn('UiButton(saveButton,', ui)
        self.assertIn('advancedHeight.Height = advancedOpen ? 145 : 37;', ui)
        self.assertIn('var utilityButtons = new[] { repair, diagnostics, report, reportToken, selfUpdate, launchButton };', ui)
        self.assertIn('UiButton(rollbackButton, "Rollback")', ui)
        self.assertIn('UiButton(autoLootReport, "Wyślij log AutoLoot")', ui)
        self.assertIn('UiButton(monitorButton, "Szczegóły GitHub")', ui)

    def test_live_branch_badges_are_small_and_dynamic(self):
        monitor = (ROOT / "tools/updater/Updater335GitHubMonitor.cs").read_text(encoding="utf-8")
        self.assertIn("monitorStrips = UiGrid(3, 2);", monitor)
        self.assertIn("Arrange335MonitorBadges(branches)", monitor)
        self.assertIn('"/branches?per_page=100"', monitor)
        self.assertIn('"branch=" + Uri.EscapeDataString(b)', monitor)
        self.assertIn("i < branches.Count && i < 6", monitor)
        self.assertIn("Interval = 20000", monitor)

    def test_module_list_comes_only_from_installed_state(self):
        ui = (ROOT / "tools/updater/Updater335Dashboard.cs").read_text(encoding="utf-8")
        code = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        self.assertIn('AsArray(GetValue(installed, "managed_files"))', ui)
        self.assertIn('Update335ModuleSummary(installed);', code)
        self.assertIn('Update335ModuleSummary(null);', code)
        self.assertIn('gameDir.Leave += delegate { SaveConfig(false); RefreshLocalState(); };', ui)
        self.assertIn('token.Leave += delegate { SaveConfig(false); };', ui)

if __name__ == "__main__":
    unittest.main()
