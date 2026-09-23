import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_module_registry import validate

def fixture():
    rt = {"files": [
        {"kind": "dll", "component": "Core", "depends_on": []},
        {"kind": "dll", "component": "Client", "depends_on": ["Core"]},
    ]}
    module = lambda name, deps: {
        "component": name, "sources": ["src/README.md"], "requires": deps,
        "resources": [], "build": {"toolchain": "msvc_x86",
            "sources": ["src/README.md"], "include_dirs": [],
            "libraries": [], "cflags": [], "ldflags": []}}
    registry = {"schema_version": 1, "target_build": 12340,
                "modules": [module("Core", []), module("Client", ["Core"])]}
    return rt, registry

class RegistryTests(unittest.TestCase):
    def test_valid_dependencies(self):
        self.assertEqual(validate(*fixture()), [])
    def test_unregistered_module_fails(self):
        rt, rg = fixture()
        rg["modules"].pop()
        self.assertTrue(any("missing Client" in s for s in validate(rt, rg)))
    def test_dependency_load_order_fails(self):
        rt, rg = fixture()
        rt["files"].reverse()
        self.assertTrue(any("load first" in s for s in validate(rt, rg)))
    def test_conflicting_exclusive_hook_fails(self):
        rt, rg = fixture()
        for mod in rg["modules"]:
            mod["resources"] = [{"id": "wow12340:0x00123456:move", "mode": "exclusive"}]
        self.assertTrue(any("conflicting writers" in s for s in validate(rt, rg)))
    def test_unarbitrated_chain_fails(self):
        rt, rg = fixture()
        for mod in rg["modules"]:
            mod["resources"] = [{"id": "logical:movement", "mode": "chain",
                                  "arbitrator": "MISSING"}]
        self.assertTrue(any("arbitrator" in s for s in validate(rt, rg)))
    def test_observer_does_not_conflict(self):
        rt, rg = fixture()
        rg["modules"][0]["resources"] = [{"id": "win32:WndProc", "mode": "exclusive"}]
        rg["modules"][1]["resources"] = [{"id": "win32:WndProc", "mode": "observe"}]
        self.assertEqual(validate(rt, rg), [])

if __name__ == "__main__":
    unittest.main()
