"""Silent AutoLoot diagnostic addon remains usable with the Epoch updater."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class EpochSilentDiagnosticTests(unittest.TestCase):
    def test_chat_suppressed_but_event_collection_preserved(self):
        source = (ROOT / "src/AutoLoot/diagnostics/WoW335AutoLootDiag/WoW335AutoLootDiag.lua").read_text(encoding="utf-8")
        self.assertIn("local function Print(_) end", source)
        self.assertNotIn("DEFAULT_CHAT_FRAME:AddMessage", source)
        self.assertIn("LOOT_OPENED", source)
        self.assertIn("WoW335AutoLootDiagLog", source)

    def test_epoch_refreshes_only_untouched_managed_addon(self):
        feature = (ROOT / "tools/updater/UpdaterAutoLootDiagFeature.cs").read_text(encoding="utf-8")
        app = (ROOT / "tools/updater/WoW335Updater.cs").read_text(encoding="utf-8")
        self.assertIn("RefreshManagedIfPresent(string root, Assembly assembly)", feature)
        self.assertIn("marker.Length != 4", feature)
        self.assertIn("HashFile(Path.Combine(addonDir, ScriptName))", feature)
        self.assertIn("Install(root, assembly, true)", feature)
        self.assertIn("AutoLootDiagSupport.RefreshManagedIfPresent(", app)
        self.assertIn("var epochUpdated = await EpochInstallAsync();", app)


if __name__ == "__main__":
    unittest.main()
