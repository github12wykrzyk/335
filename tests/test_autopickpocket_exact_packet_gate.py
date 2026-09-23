"""Assert packet and native fallback ABI gates match this exact pinned PE32 x86 client.

No capstone dependency: this is a mandatory cross-platform CI unit test.
A source-only wrong opcode gate previously passed compilation but disabled PP
in-game. The runtime must never ship with mismatching audited code signatures.
"""
import hashlib
import json
import re
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITES = {
    "send_head": 0x006B0B50,
    "native_head": 0x00632B50,
    "caller_head": 0x0080B4E3,
    "cast_prefix": 0x0080DA40,
    "pos_prefix": 0x006E6F10,
    "caller_postfix": 0x00510423,
}

class ExactPacketGateTests(unittest.TestCase):
    def test_every_runtime_opcode_gate_matches_exact_client(self):
        exe = (ROOT / "Wow.exe").read_bytes()
        target = json.loads((ROOT / "runtime/client_exe_target.json").read_text())
        self.assertEqual(hashlib.sha256(exe).hexdigest(), target["sha256"])
        self.assertEqual(len(exe), target["size"])
        pe, = struct.unpack_from("<I", exe, 0x3c)
        self.assertEqual(exe[:2], b"MZ")
        self.assertEqual(exe[pe:pe+4], b"PE\\x00\\x00".decode("unicode_escape").encode("latin1"))
        self.assertEqual(struct.unpack_from("<H", exe, pe+4)[0], 0x14c)
        opt = pe + 24
        self.assertEqual(struct.unpack_from("<H", exe, opt)[0], 0x10b)
        base, = struct.unpack_from("<I", exe, opt+28)
        count, = struct.unpack_from("<H", exe, pe+6)
        header_size, = struct.unpack_from("<H", exe, pe+20)
        sections = []
        for i in range(count):
            header = opt + header_size + i*40
            virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII",exe,header+8)
            flags, = struct.unpack_from("<I",exe,header+36)
            if flags & 0x20000000:
                sections.append((base+rva, min(virtual_size,raw_size), raw))
        def read(va, n):
            for address, size, offset in sections:
                if address <= va and va+n <= address+size:
                    return exe[offset+va-address:offset+va-address+n]
            self.fail("unmapped runtime gate 0x%08X" % va)
        host = (ROOT / "src/AutoPickPocket/autopickpocket_win32_host.c").read_text()
        for name, address in SITES.items():
            match = re.search(r"static const BYTE " + name + r"\\[\\]\\s*=\\s*\\{([^}]*)\\}", host)
            self.assertIsNotNone(match, "missing ABI gate: " + name)
            expected = bytes(int(token,16) for token in
                             re.findall(r"0x[0-9a-fA-F]{2}", match.group(1)))
            self.assertTrue(expected, "empty ABI gate: " + name)
            self.assertEqual(read(address,len(expected)), expected,
                             "mismatching runtime gate " + name)
        rel, = struct.unpack("<i",read(0x0080B4EF,4))
        self.assertEqual(0x0080B4F3 + rel, SITES["send_head"])

if __name__ == "__main__":
    unittest.main()
