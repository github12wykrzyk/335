"""Regression: isolated adapter may pass mock tests, never game-delivery gate."""
import json
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from autopickpocket_delivery import inspect

ROOT = Path(__file__).resolve().parents[1]

class AutoPickPocketDeliveryTests(unittest.TestCase):
    def test_current_isolated_adapter_not_considered_playable(self):
        blockers = inspect(ROOT)
        self.assertTrue(any("ACTIVE_RUNTIME" in s for s in blockers))
        self.assertTrue(any("GAME_POLICY" in s or "MISSING_SOURCE: src/AutoPickPocket/autopickpocket_game_policies.c" in s
                            for s in blockers))
        self.assertTrue(any("GAME_THREAD_LOADER" in s or "MISSING_SOURCE: src/Loader12340/Wow335Loader.c" in s
                            for s in blockers))
        self.assertTrue(any("UPDATER" in s for s in blockers))
    def test_empty_fixture_rejected_not_exception(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIn("MISSING_SOURCE: runtime/current.json", inspect(Path(tmp)))
    def test_delivery_workflow_does_not_publish_before_native_gate(self):
        wf = (ROOT / ".github/workflows/autopickpocket_delivery.yml").read_text(encoding="utf-8")
        self.assertLess(wf.index("python tools/autopickpocket_delivery.py"),
                        wf.index("python tools/build_active.py"))
        self.assertLess(wf.index("python tools/build_active.py"),
                        wf.index("python tools/verify_candidate_package.py"))
        self.assertLess(wf.index("python tools/verify_candidate_package.py"),
                        wf.index("name: WoW335-AUTOPICKPOCKET-TEST-"))
        self.assertIn("if: always()", wf)  # exact-SHA blocker report even on failure
        self.assertIn("FINAL_PACKAGE gate", wf)
        self.assertNotIn("continue-on-error:", wf)

if __name__ == "__main__":
    unittest.main()
