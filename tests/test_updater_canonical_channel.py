"""One canonical work updater supports every branch without replacing game-package gates."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class CanonicalUpdaterChannelTests(unittest.TestCase):
    def test_only_work_publishes_an_updater(self):
        workflow = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        self.assertIn("    if: github.ref_name == 'work'", workflow)
        self.assertIn("name: WoW335Updater-${{ github.sha }}", workflow)
        self.assertNotIn("WoW335Updater-EXPERIMENT-", workflow)
        self.assertIn("github.rest.actions.deleteArtifact", workflow)
        self.assertIn("artifact.name === keep", workflow)
        self.assertIn("  actions: write", workflow)
        self.assertNotIn("      - main", workflow)
        self.assertNotIn("      - 'feature/**'", workflow)

    def test_self_update_is_independent_of_selected_game_branch(self):
        updater = (ROOT / "tools/updater/UpdaterMaintenanceFeature.cs").read_text(encoding="utf-8")
        self.assertIn('var branch = "work";', updater)
        self.assertNotIn('channel.SelectedIndex == 1 ? "main" : "work"', updater)
        self.assertIn('UpdaterArtifactPrefix + liveHead', updater)
        self.assertIn('GetString(meta, "git_sha"), liveHead', updater)
        self.assertIn('GetString(meta, "channel"), branch', updater)

    def test_game_branch_selection_keeps_exact_sha_and_manifest_gates(self):
        code = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        self.assertIn("RefreshBranchChoicesAsync()", code)
        self.assertIn('"/branches?per_page=100&page=" + page', code)
        self.assertIn('var branch = SelectedGameBranch();', code)
        self.assertIn('state["branch"] = remote.Branch;', code)
        self.assertIn('var expectedBranch = remote.Branch;', code)
        self.assertIn('GetString(chosen, "head_sha"), liveHead', code)
        self.assertIn('GetString(meta, "branch") != expectedBranch', code)
        self.assertIn("UpdaterBuildInfo.PinnedClientSha256", code)


if __name__ == "__main__":
    unittest.main()
