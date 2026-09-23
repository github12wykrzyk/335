"""Regression tests for the feature/loader-12340 updater-only TEST wiring."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class EpochUpdaterTests(unittest.TestCase):
    def test_updater_build_compiles_explicit_epoch_feature(self):
        wf=(ROOT/".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        ui=(ROOT/"tools/updater/Updater335Dashboard.cs").read_text(encoding="utf-8")
        app=(ROOT/"tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        feature=(ROOT/"tools/updater/UpdaterEpochLoaderFeature.cs").read_text(encoding="utf-8")
        self.assertIn("feature/loader-12340",wf)
        maintenance=(ROOT/"tools/updater/UpdaterMaintenanceFeature.cs").read_text(encoding="utf-8")
        self.assertIn('UpdaterBuildInfo.Version.EndsWith("-epoch-test"',maintenance)
        self.assertIn('"feature/loader-12340"',maintenance)
        self.assertIn("UpdaterEpochLoaderFeature.cs",wf)
        self.assertIn("Epoch Loader TEST",ui)
        self.assertIn("Aktywne DLL / kolejność",ui)
        self.assertIn("Przywróć Epoch DLL",ui)
        self.assertIn("EpochValidateLaunch(root);",app)
        self.assertIn("EpochBranch = \"feature/loader-12340\"",feature)
        self.assertIn("UpdaterSafety.RequireLatestSuccessfulRun",feature)
        self.assertIn("EpochOriginalSha",feature)
        self.assertIn("EpochCheckFile",feature)
        self.assertIn("EpochRollback()",feature)
        self.assertIn("test_pair_gate",feature)
        self.assertIn("module_load_order",feature)
        self.assertIn("runtime/current.json",feature)
        self.assertNotIn("github12wykrzyk/wow112",feature)
        self.assertNotIn("github12wykrzyk/frostmourne",feature)

    def test_empty_runtime_does_not_allow_arbitrary_dll_enumeration(self):
        manifest=json.loads((ROOT/"runtime/current.json").read_text(encoding="utf-8"))
        loader=(ROOT/"src/Loader12340/Wow335Loader.c").read_text(encoding="utf-8")
        self.assertEqual(manifest["files"],[])
        self.assertIn("MANIFEST_CREATED_EMPTY",loader)
        self.assertIn("CREATE_NEW",loader)
        self.assertIn("LOAD_WITH_ALTERED_SEARCH_PATH",loader)
        self.assertIn("DUPLICATE",loader)
        self.assertNotIn("FindFirstFile",loader)

if __name__=="__main__":
    unittest.main()
