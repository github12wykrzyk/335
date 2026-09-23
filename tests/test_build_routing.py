import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from build_active import select_modules, inspect_dll

class BuildRoutingTests(unittest.TestCase):
    def test_all_components_in_full_build(self):
        registry = {"modules":[{"component":"Core"},{"component":"Client"}]}
        self.assertEqual(select_modules(registry, None), {"Core","Client"})
    def test_non_pe_cannot_be_a_game_dll(self):
        with self.assertRaises(ValueError):
            inspect_dll(Path(__file__))

if __name__ == "__main__":
    unittest.main()
