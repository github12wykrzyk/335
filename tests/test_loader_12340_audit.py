import hashlib
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from audit_loader_12340 import audit, inspect_imports


def sample_pe():
    data = bytearray(1536)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3c, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, 0x14c, 1, 0, 0, 0, 224, 2)
    opt = 0x98
    struct.pack_into("<H", data, opt, 0x10b)
    struct.pack_into("<I", data, opt + 16, 0x1000)
    struct.pack_into("<I", data, opt + 28, 0x400000)
    struct.pack_into("<I", data, opt + 56, 0x2000)
    struct.pack_into("<I", data, opt + 60, 0x200)
    struct.pack_into("<H", data, opt + 68, 2)
    struct.pack_into("<I", data, opt + 92, 16)
    struct.pack_into("<II", data, opt + 104, 0x1000, 40)
    section = opt + 224
    struct.pack_into("<IIII", data, section + 8, 0x400, 0x1000, 0x400, 0x200)
    struct.pack_into("<IIIII", data, 0x200, 0x1060, 0, 0, 0x1080, 0x1060)
    struct.pack_into("<II", data, 0x260, 0x1090, 0)
    data[0x280:0x28d] = b"twloader.dll\0"
    struct.pack_into("<H", data, 0x290, 0)
    data[0x292:0x298] = b"func1\0"
    return data


class LoaderAuditTests(unittest.TestCase):
    def test_named_import_and_function(self):
        result = inspect_imports(bytes(sample_pe()))
        self.assertTrue(result["legacy_loader_imported"])
        self.assertEqual(result["imported_dlls"][0],
                         {"dll": "twloader.dll", "functions": ["func1"]})

    def test_marker_not_equivalent_to_import(self):
        data = sample_pe()
        data[0x280:0x28d] = b"otherxxx.dll\0"
        data[0x330:0x33d] = b"twloader.dll\0"
        result = inspect_imports(bytes(data))
        self.assertFalse(result["legacy_loader_imported"])
        self.assertTrue(result["legacy_loader_markers_ascii"]["twloader.dll"])

    def test_invalid_rva_rejected(self):
        data = sample_pe()
        struct.pack_into("<I", data, 0x200, 0x7fffffff)
        with self.assertRaises(ValueError):
            inspect_imports(bytes(data))

    def test_exact_client_identity_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Wow.exe"
            data = bytes(sample_pe())
            path.write_bytes(data)
            target = {"name": "Wow.exe", "size": len(data),
                      "sha256": "0" * 64, "target_build": 12340,
                      "architecture": "x86"}
            with self.assertRaisesRegex(ValueError, "selected client mismatch"):
                audit(path, target)
            target["sha256"] = hashlib.sha256(data).hexdigest()
            self.assertEqual(audit(path, target)["conclusion"],
                             "STATIC_AUDIT_ONLY_NOT_LOADER_NOT_GAME_PACKAGE")

    def test_exact_repo_client_imports_and_report(self):
        import json
        path = ROOT / "Wow.exe"
        self.assertTrue(path.is_file(), "Selected 12340 client missing from checkout")
        target = json.loads((ROOT / "runtime/client_exe_target.json").read_text(encoding="utf-8"))
        report = audit(path, target)
        self.assertEqual(report["conclusion"], "STATIC_AUDIT_ONLY_NOT_LOADER_NOT_GAME_PACKAGE")
        self.assertIsInstance(report["imports"]["imported_dlls"], list)
        print("LOADER_12340_STATIC_IMPORTS:", json.dumps(report["imports"]["imported_dlls"], sort_keys=True))
        print("LOADER_12340_LEGACY_MARKERS:", json.dumps(report["imports"]["legacy_loader_markers_ascii"], sort_keys=True))


if __name__ == "__main__":
    unittest.main()
