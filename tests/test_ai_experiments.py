import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from ai_experiments import route, validate, scope_errors

def example():
    return {"schema_version":1,"repository":"github12wykrzyk/335",
      "experiments":[{"id":"movement","goal":"Coordinate movement",
        "branch":"work","modules":["Movement"],"dependencies":[],
        "shared_resources":["logical:movement"],"status":"in_progress",
        "observed_head":None,"verified_commit":None,"tests":[],"package":None}]}

class ExperimentTests(unittest.TestCase):
    def test_empty_ledger(self):
        self.assertEqual(validate({"schema_version":1,"repository":"github12wykrzyk/335","experiments":[]}),[])
    def test_continues_owned_module(self):
        self.assertEqual(route(example(),"Movement")["branch"],"work")
    def test_isolates_new_mechanism(self):
        self.assertEqual(route(example(),"NewTarget")["branch"],"feature/newtarget")
    def test_explicit_branch_has_priority(self):
        self.assertEqual(route(example(),"Movement","parallel")["branch"],"parallel")
    def test_unknown_branch_rejected(self):
        with self.assertRaises(ValueError):
            route(example(),"Movement","main")
    def test_acceptance_needs_same_sha_test_package(self):
        ledger = example()
        e = ledger["experiments"][0]
        e["status"] = "accepted"
        e["verified_commit"] = "a"*40
        e["package"] = {"sha":"a"*40,"id":"candidate"}
        self.assertTrue(validate(ledger))
        e["tests"] = [{"sha":"a"*40,"result":"pass"}]
        self.assertEqual(validate(ledger),[])
    def test_duplicate_experiments_rejected(self):
        ledger=example()
        ledger["experiments"].append(ledger["experiments"][0])
        self.assertTrue(any("duplicate" in s for s in validate(ledger)))

    def test_feature_candidate_requires_owned_runtime_dll(self):
        ledger = example()
        ledger["experiments"][0]["branch"] = "feature/movement"
        runtime = {"files": [{"component": "AutoLoot", "kind": "dll"}]}
        self.assertIn("INCOMPLETE_EXPERIMENT", str(scope_errors(ledger, runtime, "feature/movement")))
        runtime["files"].append({"component": "Movement", "kind": "dll"})
        self.assertEqual(scope_errors(ledger, runtime, "feature/movement"), [])

    def test_feature_candidate_requires_registered_owner(self):
        self.assertTrue(scope_errors({"experiments": []}, {"files": []}, "feature/new"))
        self.assertEqual(scope_errors({"experiments": []}, {"files": []}, "work"), [])

    def test_blocked_experiment_cannot_ship_candidate(self):
        ledger = example()
        ledger["experiments"][0]["branch"] = "feature/movement"
        ledger["experiments"][0]["status"] = "blocked"
        self.assertTrue(scope_errors(ledger, {"files": [{"component": "Movement", "kind": "dll"}]}, "feature/movement"))

if __name__ == "__main__":
    unittest.main()
