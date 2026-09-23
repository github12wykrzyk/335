"""Retire the Epoch updater without leaving two simultaneous loader mechanisms."""
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class EpochToWorkBridgeTests(unittest.TestCase):
    def test_legacy_user_gets_one_time_bridge_and_canonical_work_next(self):
        app=(ROOT/"tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        maintenance=(ROOT/"tools/updater/UpdaterMaintenanceFeature.cs").read_text(encoding="utf-8")
        feature=(ROOT/"tools/updater/UpdaterEpochLoaderFeature.cs").read_text(encoding="utf-8")
        self.assertIn('"0.3.19-335-epoch-test"', app)
        self.assertIn("Shown += async delegate",app)
        self.assertIn('UpdaterBuildInfo.Version != "0.3.19-335-epoch-test"',maintenance)
        self.assertIn('Branch = branch',maintenance)
        self.assertIn('HeadSha = liveHead',maintenance)
        self.assertIn('remote.Branch == "work"',maintenance)
        self.assertIn('await game.EpochPrepareWorkTransitionAsync(remote.HeadSha)',maintenance)
        self.assertIn('await FindLatestPackageAsync()',feature)
        self.assertIn('available.HeadSha, targetSha',feature)
        self.assertIn('EpochDetachForWork()',feature)
        self.assertIn('EpochStageFile(orig, epoch, EpochOriginalSha)',feature)
        self.assertIn('File.Delete(loader)',feature)
        self.assertIn('File.Delete(EpochStatePath(root))',feature)
        self.assertIn('EpochValidateLaunch(root)',feature)
