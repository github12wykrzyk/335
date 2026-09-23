"""One canonical updater track, with no feature artifact masquerading as TEST."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class CanonicalUpdaterChannelTests(unittest.TestCase):
    def test_only_work_or_main_publish_auto_installable_updater(self):
        workflow = (ROOT / ".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        self.assertIn("if: github.ref_name == 'work' || github.ref_name == 'main'", workflow)
        self.assertIn("if: github.ref_name != 'work' && github.ref_name != 'main'", workflow)
        self.assertIn("name: WoW335Updater-EXPERIMENT-${{ github.sha }}", workflow)
        self.assertIn("name: WoW335Updater-${{ github.sha }}", workflow)
    def test_normal_test_channel_is_work(self):
        updater = (ROOT / "tools/updater/UpdaterMaintenanceFeature.cs").read_text(encoding="utf-8")
        self.assertNotIn('"feature/loader-12340"', updater)
        self.assertIn('"work"', updater)
        self.assertIn('UpdaterArtifactPrefix + liveHead', updater)

if __name__ == "__main__":
    unittest.main()
