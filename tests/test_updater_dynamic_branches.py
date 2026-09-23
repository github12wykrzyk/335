"""Source regression checks for dynamically discovered GitHub branches.

These checks do not claim any particular branch workflow passed or that a
feature branch is installable. They guard UI discovery and its security boundary.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class DynamicBranchMonitorTests(unittest.TestCase):
    def test_main_window_uses_dynamic_scrollable_branch_badges(self):
        source = (ROOT / "tools/updater/Updater335GitHubMonitor.cs").read_text(encoding="utf-8")
        dashboard = (ROOT / "tools/updater/Updater335Dashboard.cs").read_text(encoding="utf-8")
        self.assertIn("FlowLayoutPanel monitorLayout", source)
        self.assertIn("monitorLayout.AutoScroll = true;", source)
        self.assertIn("Rebuild335MonitorBadges(names);", source)
        self.assertIn("monitorLayout.Controls.Add(outline);", source)
        self.assertIn("monitorLayout.Resize += delegate { Resize335MonitorBadges(); };", source)
        self.assertIn("header.Controls.Add(Build335MonitorHeader(), 1, 0);", dashboard)
        self.assertIn("badge.Click += delegate { Open335Monitor(); };", source)
        self.assertNotIn('foreach (var b in new[] { "work", "main" })', source)

    def test_all_branches_are_discovered_without_auto_authorizing_installation(self):
        source = (ROOT / "tools/updater/Updater335GitHubMonitor.cs").read_text(encoding="utf-8")
        app = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        self.assertIn('"/branches?per_page=100&page=" + page', source)
        self.assertIn("page <= 10", source)
        self.assertIn("entries.Length < 100", source)
        self.assertIn("monitorBranchesFetchedUtc", source)
        self.assertIn("monitorHeads.Clear();", source)
        self.assertIn("foreach (var entry in monitorHeads)", source)
        self.assertIn("Recent335RunsAsync(client)", source)
        self.assertIn('"/actions/runs?per_page=100&page=" + page', source)
        self.assertIn('GetString(row, "head_branch"), branch', source)
        self.assertIn('GetString(row, "head_sha"), sha', source)
        self.assertIn("!seenWorkflows.Add(workflow)", source)
        self.assertIn("monitorBranchListTruncated", source)
        self.assertIn('private bool UseEpochTestFlow()', app)
        self.assertIn('return await EpochInstallAsync();', app)
        self.assertNotIn('return await EpochInstallAsync();', source)

    def test_displayed_status_is_not_equated_to_runnable_package(self):
        source = (ROOT / "tools/updater/Updater335GitHubMonitor.cs").read_text(encoding="utf-8")
        self.assertIn("Status CI nie oznacza gotowej paczki ani uprawnienia do instalacji.", source)
        self.assertIn("Nie utożsamiaj starego sukcesu z bieżącym HEAD.", source)
        self.assertIn('Set335Badge(branch, "UNKNOWN", "", monitorReport);', source)


if __name__ == "__main__":
    unittest.main()
