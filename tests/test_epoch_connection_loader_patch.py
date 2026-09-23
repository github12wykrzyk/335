import hashlib
import struct
import sys
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from epoch_connection_loader_patch import patch_epoch
from audit_loader_12340 import inspect_imports
from test_loader_12340_audit import sample_pe


def epoch_test_fixture():
    """Synthetic PE32 DLL with import/export/TLS/relocation directory fixtures."""
    data=sample_pe()
    opt=0x98
    struct.pack_into("<H",data,0x80+4+18,0x2002)
    struct.pack_into("<II",data,opt+32,0x1000,0x200)
    struct.pack_into("<I",data,opt+60,0x200)
    struct.pack_into("<I",data,opt+56,0x2000)
    for idx,rva,length in ((0,0x11c0,40),(5,0x1210,16),
                           (6,0x1230,28),(9,0x11e8,24)):
        struct.pack_into("<II",data,opt+96+idx*8,rva,length)
    data[0x3c0:0x3e8]=bytes(range(40))
    data[0x3e8:0x400]=bytes(range(24))
    data[0x410:0x420]=bytes(range(16))
    data[0x430:0x44c]=bytes(range(28))
    return bytes(data)


class EpochConnectionImportTests(unittest.TestCase):
    def test_keeps_existing_imports_and_all_original_sections(self):
        data=epoch_test_fixture()
        patched,report=patch_epoch(data,hashlib.sha256(data).hexdigest())
        self.assertEqual(report["kind"],"EPOCH_STARTUP_IMPORT_COPY_ONLY_NOT_GAME_PACKAGE")
        self.assertEqual(report["final_package"],"NOT_RUN")
        self.assertTrue(all(report[k] for k in (
            "original_exports_preserved","original_tls_preserved",
            "original_relocations_preserved","original_sections_preserved")))
        self.assertEqual(inspect_imports(patched)["imported_dlls"],
                         inspect_imports(data)["imported_dlls"]+
                         [{"dll":"Wow335Loader.dll","functions":["#1"]}])
        self.assertEqual(patched[0x200:0x600],data[0x200:0x600])
        self.assertEqual(epoch_test_fixture(),data)

    def test_rejects_wrong_source_identity(self):
        with self.assertRaisesRegex(ValueError,"SHA256 mismatch"):
            patch_epoch(epoch_test_fixture(),"0"*64)

    def test_rejects_occupied_section_header(self):
        data=bytearray(epoch_test_fixture())
        data[0x1a0]=0x7e
        with self.assertRaisesRegex(ValueError,"no vacant section-header slot"):
            patch_epoch(bytes(data),hashlib.sha256(data).hexdigest())

    def test_rejects_security_directory(self):
        data=bytearray(epoch_test_fixture())
        struct.pack_into("<II",data,0x98+96+4*8,0x400,0x10)
        with self.assertRaisesRegex(ValueError,"security directory"):
            patch_epoch(bytes(data),hashlib.sha256(data).hexdigest())

    def test_rejects_unclassified_overlay(self):
        data=epoch_test_fixture()+bytes(32)
        with self.assertRaisesRegex(ValueError,"unexpected overlay"):
            patch_epoch(data,hashlib.sha256(data).hexdigest())


if __name__=="__main__":
    unittest.main()
