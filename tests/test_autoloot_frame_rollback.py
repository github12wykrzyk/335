"""Rollback the failed frame build; never promote its byte identity as a valid runtime."""
import json
import unittest
from pathlib import Path
R = Path(__file__).resolve().parents[1]

class FailedFrameRollbackTests(unittest.TestCase):
    def test_exact_prior_work_pe32_and_origin(self):
        meta=json.loads((R/"runtime/epoch_loader_interop.json").read_text(encoding="utf-8"))
        s=(R/"tools/updater/UpdaterEpochLoaderFeature.cs").read_text(encoding="utf-8")
        wf=(R/".github/workflows/build_updater.yml").read_text(encoding="utf-8")
        self.assertEqual(meta["source"]["branch"], "work")
        self.assertEqual(meta["source"]["commit"], "3cbc14b66aa2ed5687b0132190c4381494c0bdb6")
        self.assertEqual(meta["source"]["sha256"], "6551b34fde100edaf0b9597b4e267d1844acf92da379fae344c41efeaa8c31c3")
        self.assertIn("EpochFrameAutoLootCommit",s)
        self.assertIn("EpochCheckFile(target, oldHash)",s)
        self.assertIn("if (oldCommit == EpochWorkAutoLootCommit)",s)
        self.assertIn("EpochCheckFile(nextDll, EpochWorkAutoLootSha);",s)
        self.assertIn("EpochStageFile(nextDll, target, EpochWorkAutoLootSha);",s)
        self.assertIn('workState.Remove("origin_artifact_kind")',s)
        self.assertIn('workState["head_sha"] = EpochWorkAutoLootCommit;',s)
        self.assertIn('epochState["source_commit"] = EpochWorkAutoLootCommit;',s)
        self.assertIn("EpochFrameAutoLootSha",s)  # accepted only as migration source
        self.assertIn("File.Copy(target, backupDll)",s)
        self.assertIn("File.Copy(workPath, backupWork)",s)
        self.assertIn("File.Copy(epochPath, backupEpoch)",s)
        self.assertIn("EpochStageFile(backupDll, target, oldHash)",s)
        self.assertIn("EpochInterop.AutoLoot335.dll",wf)

    def test_corresponding_pre_frame_loader_and_update_flow(self):
        src=(R/"src/Loader12340/Wow335Loader.c").read_text(encoding="utf-8")
        app=(R/"tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        version=(R/"tools/updater/UpdaterSafety.cs").read_text(encoding="utf-8")
        self.assertIn('HOOK_ENABLED_RESPONSIVE',src)
        self.assertNotIn('FRAME_ARMING',src)
        self.assertNotIn('AL335_FrameStatus',src)
        self.assertIn('EpochWorkAutoLootCommit',app)
        self.assertNotIn('GetString(installedEpoch, "source_commit") == EpochFrameAutoLootCommit',app)
        self.assertIn('Version = "0.3.18-335-epoch-test"',version)

if __name__ == "__main__":
    unittest.main()
