import hashlib
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from loader_patch_12340 import patch
from test_loader_12340_audit import sample_pe
from audit_loader_12340 import inspect_imports


def target_fixture():
    data = sample_pe()
    opt = 0x98
    struct.pack_into("<III", data, opt + 32, 0x1000, 0x200, 0x2000)
    struct.pack_into("<I", data, opt + 60, 0x200)
    struct.pack_into("<I", data, 0x200 + 12, 0x10a0)  # original DLL name RVA
    data[0x2a0:0x2b4] = b"EpochConnection.dll\0"
    return bytes(data)


class StartupImportPreviewTests(unittest.TestCase):
    def test_preserves_imports_and_original_exe(self):
        data = target_fixture()
        expected = hashlib.sha256(data).hexdigest()
        patched, report = patch(data, expected)
        self.assertEqual(data, target_fixture())
        self.assertEqual(report["original_sha256"], expected)
        self.assertEqual(report["final_package"], "NOT_RUN")
        self.assertEqual(inspect_imports(patched)["imported_dlls"],
                         inspect_imports(data)["imported_dlls"] +
                         [{"dll": "Wow335Loader.dll", "functions": ["#1"]}])
        self.assertTrue(report["original_epochconnection_preserved"])

    def test_refuses_wrong_exe(self):
        with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
            patch(target_fixture(), "0"*64)

    def test_refuses_occupied_section_header(self):
        data = bytearray(target_fixture())
        data[0x1a0] = 42
        with self.assertRaisesRegex(ValueError, "section-header"):
            patch(bytes(data), hashlib.sha256(data).hexdigest())

    def test_refuses_signed_client(self):
        data = bytearray(target_fixture())
        struct.pack_into("<II", data, 0x98 + 96 + 4*8, 0x500, 16)
        with self.assertRaisesRegex(ValueError, "signed EXE"):
            patch(bytes(data), hashlib.sha256(data).hexdigest())

    def test_refuses_twloader_repatch(self):
        data = bytearray(target_fixture())
        data[0x2a0:0x2b4] = b"twloader.dll\0".ljust(20, b"\0")
        with self.assertRaisesRegex(ValueError, "already contains"):
            patch(bytes(data), hashlib.sha256(data).hexdigest())


if __name__ == "__main__":
    unittest.main()
