"""Actions must publish the genuine isolated PE32 adapter, not a game candidate."""
import json
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class AutoPickPocketArtifactTests(unittest.TestCase):
    def test_isolated_adapter_and_sha_report_delivered(self):
        workflow = (ROOT / ".github/workflows/build_work_candidate.yml").read_text(encoding="utf-8")
        self.assertIn("AutoPickPocket-ADAPTER-ISOLATED-${{ github.sha }}", workflow)
        self.assertIn("dist/pp_core_experiment.json", workflow)
        self.assertIn("dist/pp_core_experiment/AutoPickPocket335_ADAPTER_ISOLATED.dll", workflow)
        self.assertIn("if: steps.pp_route.outputs.core_only == 'true'", workflow)
        self.assertIn("NO GAME PACKAGE", workflow)
    def test_isolated_module_is_not_registered_for_game(self):
        manifest = json.loads((ROOT / "runtime/current.json").read_text(encoding="utf-8"))
        self.assertFalse(any(file.get("component") == "AutoPickPocket" for file in manifest["files"]))
if __name__ == "__main__":
    unittest.main()
