"""Updater selects only exact-SHA full PP TEST artifacts and refuses false game launch."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
class PickPocketUpdaterChannelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
    def test_explicit_separate_test_channel_preserves_work_and_main(self):
        self.assertIn('"TEST (work)", "STABLE (main)", "TEST (AutoPickPocket)"', self.source)
        self.assertIn('PpTestBranch = "feature/autopickpocket-12340"', self.source)
        self.assertIn('PpTestWorkflowName = "AutoPickPocket 12340 full TEST delivery"', self.source)
        self.assertIn('PpTestArtifactPrefix = "WoW335-AUTOPICKPOCKET-TEST-"', self.source)
        self.assertIn('PpTestInnerZip = "WoW335_AUTOPICKPOCKET_TEST.zip"', self.source)
        self.assertIn('Channel = stable ? "stable" : ppTest ? "pp_test" : "test"', self.source)
    def test_live_head_and_exact_branch_and_manifest_verified(self):
        self.assertIn('Uri.EscapeDataString(branch)', self.source)
        self.assertIn('remote.Channel == "pp_test" ? PpTestBranch : "work"', self.source)
        self.assertIn('UpdaterSafety.RequireLatestSuccessfulRun(runs, workflowName, branch)', self.source)
        self.assertIn('!string.Equals(GetString(chosen, "head_sha"), liveHead', self.source)
    def test_feature_branch_builds_the_updater(self):
        wf = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        self.assertIn("'feature/autopickpocket-12340'", wf)
    def test_pp_cannot_launch_via_autoloot_only_fallback(self):
        guard = self.source.index('AutoPickPocket TEST: wymagany jest zweryfikowany wspólny loader.')
        launch = self.source.index('if (!TryLaunchInstalledAutoLoot(root, exe, state))')
        self.assertLess(guard, launch)
        self.assertIn('AutoPickPocket335.dll', self.source)
if __name__ == "__main__":
    unittest.main()
