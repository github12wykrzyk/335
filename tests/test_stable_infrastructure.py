"""Main infrastructure bootstrap cannot be mislabeled as a playable stable game release."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class StableInfrastructureTests(unittest.TestCase):
    def test_no_active_stable_game_modules_before_exact_sha_acceptance(self):
        current = json.loads((ROOT / "CURRENT.json").read_text(encoding="utf-8"))
        runtime = json.loads((ROOT / "runtime/current.json").read_text(encoding="utf-8"))
        registry = json.loads((ROOT / "runtime/module_registry.json").read_text(encoding="utf-8"))
        index = json.loads((ROOT / "AI_INDEX.json").read_text(encoding="utf-8"))
        self.assertEqual(current["status"], "infrastructure-only")
        self.assertIsNone(current["stable_baseline"])
        self.assertEqual(runtime["state"], "empty")
        self.assertEqual(runtime["files"], [])
        self.assertEqual(runtime["compatibility_sets"], [])
        self.assertEqual(registry["modules"], [])
        self.assertEqual(index["modules"], [])

    def test_stable_workflows_fail_closed_on_empty_game_runtime(self):
        for name in ("pre_promote_stable.yml", "build_stable_candidate.yml"):
            text = (ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")
            self.assertIn("STABLE_INFRA_ONLY: PASS; NO_GAME_PACKAGE", text)
            self.assertIn("steps.mode.outputs.ready == 'true'", text)
            self.assertIn("verify_exact_runtime_artifacts.py", text)
            self.assertIn("verify_candidate_package.py", text)

    def test_main_updater_uses_stable_channel_without_epoch_test_activation(self):
        safety = (ROOT / "tools" / "updater" / "UpdaterSafety.cs").read_text(encoding="utf-8")
        app = (ROOT / "tools" / "updater" / "WoW335Updater.cs").read_text(encoding="utf-8")
        self.assertIn('Version = "0.3.19-335-stable-infra"', safety)
        self.assertIn('return !IsStable() &&', app)
        self.assertIn('Version.EndsWith("-epoch-test"', app)
        self.assertIn("channel.SelectedIndex = 1;", app)
        self.assertIn("TEST (work)", app)


if __name__ == "__main__":
    unittest.main()
