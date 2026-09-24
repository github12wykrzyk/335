import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from stage_esp_candidate import prepare_registration, esp_contract
from verify_module_registry import validate

def fixture():
    runtime = {"target": {"build": 12340}, "files": [
        {"component": "Client12340", "kind": "exe"},
        {"component": "AutoLoot", "kind": "dll", "depends_on": []}],
        "compatibility_sets": []}
    registry = {"schema_version": 1, "target_build": 12340, "modules": [{
        "component": "AutoLoot", "sources": ["src/README.md"],
        "requires": [], "resources": [
            {"id": "win32:WH_GETMESSAGE", "mode": "exclusive"},
            {"id": "win32:WH_CALLWNDPROC", "mode": "exclusive"}],
        "build": {"toolchain": "msvc_x86", "sources": ["src/README.md"],
                  "include_dirs": [], "libraries": [], "cflags": [],
                  "ldflags": []}}]}
    index = {"modules": [{"component": "AutoLoot"}]}
    return runtime, registry, index

class ESPStageTests(unittest.TestCase):
    def test_exact_x86_registration_and_hook_arbiter(self):
        original = fixture()
        runtime, registry, index = prepare_registration(
            *original, "a" * 64)
        self.assertEqual(len(original[1]["modules"]), 1)
        self.assertEqual(runtime["files"][-1]["sha256"], "a" * 64)
        self.assertEqual([m["component"] for m in index["modules"]],
                         ["AutoLoot", "PlayerESP"])
        self.assertEqual(validate(runtime, registry), [])
        self.assertEqual(esp_contract()["build"]["toolchain"], "msvc_x86")
        self.assertIn("d3d9.lib",esp_contract()["build"]["libraries"])
        self.assertIn("src/PlayerESP112Port/esp112_frame_hook.c",esp_contract()["build"]["sources"])
        self.assertIn("src/PlayerESP112Port/esp112_frame_draw.c",esp_contract()["build"]["sources"])
        self.assertEqual(sum(r["id"]=="win32:d3d9-endscene-code-detour" for r in esp_contract()["resources"]),1)
        self.assertIn("Gdi32.lib",esp_contract()["build"]["libraries"])
        self.assertEqual(runtime["files"][-1]["canonical_source"],"src/PlayerESP112Port/esp112_host335.c")
    def test_rejects_stale_or_fake_registration(self):
        r, g, i = fixture()
        with self.assertRaises(ValueError):
            prepare_registration(r, g, i, "fake")
        r["files"].append({"component": "PlayerESP"})
        with self.assertRaises(ValueError):
            prepare_registration(r, g, i, "b" * 64)

if __name__ == "__main__":
    unittest.main()
