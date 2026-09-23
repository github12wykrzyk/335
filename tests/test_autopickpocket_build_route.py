import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from autopickpocket_experiment import core_only, BRANCH

M = {"files": [{"component": "Client12340"}, {"component": "AutoLoot"}]}
R = {"modules": [{"component": "AutoLoot",
                  "sources": ["src/AutoLoot/autoloot_core.c",
                              "src/AutoLoot/autoloot_core.h"]}]}
CHANGED = {"src/AutoPickPocket/autopickpocket_core.c",
           "tools/updater/UpdaterIssueReportFeature.cs",
           ".github/workflows/build_work_candidate.yml",
           "tools/autopickpocket_experiment.py"}

class AutoPickPocketExperimentRouteTests(unittest.TestCase):
    def test_source_only_pp_runs_isolated(self):
        self.assertTrue(core_only(BRANCH, CHANGED, M, R))

    def test_main_and_work_always_require_full_build(self):
        self.assertFalse(core_only("work", CHANGED, M, R))
        self.assertFalse(core_only("main", CHANGED, M, R))

    def test_registering_pp_requires_full_game_candidate(self):
        manifest = {"files": M["files"] + [{"component": "AutoPickPocket"}]}
        self.assertFalse(core_only(BRANCH, CHANGED, manifest, R))
        registry = {"modules": R["modules"] + [{"component": "AutoPickPocket", "sources": []}]}
        self.assertFalse(core_only(BRANCH, CHANGED, M, registry))

    def test_active_source_and_build_gates_cannot_be_skipped(self):
        for path in ("src/AutoLoot/autoloot_core.c",
                     "src/AutoLoot/new_native_header.h",
                     "runtime/current.json", "runtime/module_registry.json",
                     "tools/build_active.py", "tools/verify_candidate_package.py",
                     "random/unrelated_file.txt"):
            with self.subTest(path=path):
                self.assertFalse(core_only(BRANCH, CHANGED | {path}, M, R))

    def test_no_pp_changes_not_isolated(self):
        self.assertFalse(core_only(BRANCH, {"tools/updater/UpdaterIssueReportFeature.cs"}, M, R))
if __name__=="__main__":
    unittest.main()
