import importlib.util
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / (name + ".py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

class ExeAuditTests(unittest.TestCase):
    def test_missing_exe_fails(self):
        with self.assertRaises(FileNotFoundError):
            load("verify_client_exe").inspect_exe(ROOT / "tests/DOES_NOT_EXIST.exe")

    def test_rejects_fake_and_pe64(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "Wow.exe"
            p.write_bytes(b"MZ" + bytes(510))
            with self.assertRaises(ValueError):
                load("verify_client_exe").inspect_exe(p)
            data = bytearray(1024)
            data[:2] = b"MZ"
            struct.pack_into("<I", data, 0x3C, 128)
            data[128:132] = b"PE\\0\\0"
            struct.pack_into("<HHIIIHH", data, 132, 0x8664, 1, 0, 0, 0, 96, 0)
            p.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "not x86"):
                load("verify_client_exe").inspect_exe(p)

    def test_empty_manifest_never_ready(self):
        ready, reason = load("candidate_readiness").assess(
            {"state": "empty", "files": []}, {"sha256": "dummy"}
        )
        self.assertFalse(ready)
        self.assertIn("EXE_ONLY", reason)

    def test_registered_client_without_module_rejected(self):
        with self.assertRaisesRegex(ValueError, "at least one real DLL"):
            load("candidate_readiness").assess(
                {"state": "candidate", "files": [{"kind": "exe", "path": "Wow.exe"}]},
                {"sha256": "dummy"},
            )

if __name__ == "__main__":
    unittest.main()
