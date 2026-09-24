import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from audit_player_esp_client import pe_layout,va_info

class EspClientAuditTests(unittest.TestCase):
    def setUp(self):
        data=bytearray(0x500)
        data[:2]=b"MZ"
        struct.pack_into("<I",data,0x3c,0x80)
        data[0x80:0x84]=b"PE\0\0"
        struct.pack_into("<H",data,0x84,0x14c)
        struct.pack_into("<H",data,0x86,1)
        struct.pack_into("<H",data,0x94,0xe0)
        struct.pack_into("<H",data,0x98,0x10b)
        struct.pack_into("<I",data,0x98+28,0x400000)
        struct.pack_into("<I",data,0x98+60,0x200)
        pos=0x98+0xe0
        data[pos:pos+8]=b".text\0\0\0"
        struct.pack_into("<IIII",data,pos+8,0x200,0x1000,0x200,0x200)
        struct.pack_into("<I",data,pos+36,0x60000020)
        data[0x220:0x223]=b"\x55\x8b\xec"
        self.raw=bytes(data)
    def test_pe_section_va_to_raw(self):
        base,headers,sections=pe_layout(self.raw)
        self.assertEqual(base,0x400000)
        self.assertEqual(headers,0x200)
        self.assertEqual(sections[0][0],".text")
        row=va_info(self.raw,0x401020)
        self.assertTrue(row["mapped"])
        self.assertEqual(row["file_offset"],0x220)
        self.assertTrue(row["executable_section"])
        self.assertTrue(row["first_16_bytes"].startswith("558bec"))
    def test_rejects_invalid_section_and_header(self):
        self.assertFalse(va_info(self.raw,0x500000)["mapped"])
        with self.assertRaises(ValueError):
            pe_layout(b"bad")

if __name__=="__main__":
    unittest.main()
