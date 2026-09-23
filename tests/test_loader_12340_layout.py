"""Read-only field audit; prints the exact client layout in GitHub CI logs."""
import hashlib
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/"tools"))
from audit_pe_layout_12340 import audit_layout
from test_loader_12340_import_patch import target_fixture


class PeLayout12340Tests(unittest.TestCase):
    def test_fixture_unoccupied_header_tail(self):
        result = audit_layout(target_fixture())
        self.assertFalse(result["header_tail_nonzero"])
        self.assertEqual(result["coff_section_count"], 1)
        self.assertEqual(result["data_directories"][1]["name"], "import")

    def test_report_exact_repo_exe_without_modification(self):
        path = ROOT/"Wow.exe"
        payload = path.read_bytes()
        expected = json.loads((ROOT/"runtime/client_exe_target.json").read_text(encoding="utf-8"))
        self.assertEqual(hashlib.sha256(payload).hexdigest(), expected["sha256"])
        layout = audit_layout(payload)
        self.assertEqual(layout["coff_section_count"], 7)
        self.assertTrue(layout["read_only"])
        print("PE_LAYOUT_12340_REPORT:", json.dumps(layout,sort_keys=True,separators=(",",":")))


if __name__=="__main__":
    unittest.main()
