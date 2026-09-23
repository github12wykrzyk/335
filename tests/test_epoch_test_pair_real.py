"""Tests against the exact EpochConnection.dll supplied in this branch."""
import hashlib
import sys
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from epoch_connection_loader_patch import PINNED_SHA, PINNED_SIZE, patch_epoch
from verify_epoch_test_pair import EXPECTED_EXPORTS, PE
from audit_loader_12340 import inspect_imports

class ExactEpochConnectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=(ROOT/"EpochConnection.dll").read_bytes()
        if len(cls.original)!=PINNED_SIZE or hashlib.sha256(cls.original).hexdigest()!=PINNED_SHA:
            raise AssertionError("GitHub EpochConnection.dll not exact pinned original")

    def test_real_pe32_x86_and_all_three_exports(self):
        original=PE(self.original)
        self.assertEqual(original.exports(),EXPECTED_EXPORTS)
        self.assertEqual(original.count,8)
        self.assertNotEqual(original.directory(5)[0],0)
        self.assertNotEqual(original.directory(9)[0],0)

    def test_real_import_patch_is_copy_only_and_deterministic(self):
        result,report=patch_epoch(self.original,PINNED_SHA)
        result2,_=patch_epoch(self.original,PINNED_SHA)
        self.assertEqual(result,result2)
        self.assertNotEqual(result,self.original)
        self.assertEqual(PE(result).exports(),EXPECTED_EXPORTS)
        self.assertEqual(inspect_imports(result)["imported_dlls"],
                         inspect_imports(self.original)["imported_dlls"]+
                         [{"dll":"Wow335Loader.dll","functions":["#1"]}])
        self.assertTrue(report["original_sections_preserved"])
        self.assertTrue(report["original_tls_preserved"])
        self.assertTrue(report["original_relocations_preserved"])
        self.assertEqual(report["final_package"],"NOT_RUN")

    def test_real_patch_rejects_second_patch_and_corrupt_source(self):
        patched,_=patch_epoch(self.original,PINNED_SHA)
        with self.assertRaisesRegex(ValueError,"already imports"):
            patch_epoch(patched,hashlib.sha256(patched).hexdigest())
        with self.assertRaisesRegex(ValueError,"SHA256 mismatch"):
            patch_epoch(self.original[:-1]+b"X",PINNED_SHA)

if __name__=="__main__":
    unittest.main()
