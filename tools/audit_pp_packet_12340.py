"""Read-only, exact-client packet transport ABI discovery; does not modify Wow.exe."""
import hashlib
import json
import struct
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
ROOT = Path(__file__).resolve().parents[1]
exe = (ROOT / "Wow.exe").read_bytes()
pin = json.loads((ROOT / "runtime/client_exe_target.json").read_text())
assert hashlib.sha256(exe).hexdigest() == pin["sha256"]
assert len(exe) == pin["size"]
assert exe[:2] == b"MZ"
pe, = struct.unpack_from("<I", exe, 0x3c)
assert exe[pe:pe+4] == b"PE\\x00\\x00".decode("unicode_escape").encode("latin1")
assert struct.unpack_from("<H", exe, pe+4)[0] == 0x14c
opt = pe+24
base, = struct.unpack_from("<I", exe, opt+28)
secs = []
for i in range(struct.unpack_from("<H", exe, pe+6)[0]):
    h = opt + struct.unpack_from("<H", exe, pe+20)[0] + i*40
    size, rva, rawsize, raw = struct.unpack_from("<IIII", exe, h+8)
    if struct.unpack_from("<I", exe, h+36)[0] & 0x20000000:
        secs.append((base+rva, exe[raw:raw+min(size,rawsize)], exe[h:h+8].split(b"\\x00")[0]))
print("EXACT_12340:", pin["sha256"], "base", hex(base))
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dump(va, before=55, after=125):
    for start, buf, name in secs:
        if start <= va < start+len(buf):
            a = max(start,va-before); b = min(start+len(buf),va+after)
            print("WINDOW", hex(va), name.decode(), "FROM", hex(a), "BYTES", buf[a-start:b-start].hex())
            for ins in md.disasm(buf[a-start:b-start], a):
                if abs(ins.address - va) <= after:
                    print(" ",hex(ins.address),ins.mnemonic,ins.op_str)
            return
    print("UNMAPPED", hex(va))
for v in [0x0080DA40, 0x0051041E]:
    dump(v,35,440)
# Exact 5-byte CALL rel32 to known audited CastSpell GUID entry.
targets = (0x0080DA40,)
for target in targets:
    hits=[]
    for start, buf, name in secs:
        for i in range(len(buf)-4):
            if buf[i] != 0xE8: continue
            rel,=struct.unpack_from("<i",buf,i+1)
            at=start+i
            if at+5+rel == target: hits.append(at)
    print("DIRECT_CALLS",hex(target),len(hits))
    for a in hits[:20]: dump(a,48,130)
# Typical CDataStore construction of CMSG_CAST_SPELL opcode (not proof of call ABI).
for key in [bytes.fromhex("682e010000"),bytes.fromhex("c7002e010000"),bytes.fromhex("2e010000")]:
    hits=[]
    for start,buf,name in secs:
        i=0
        while True:
            i=buf.find(key,i)
            if i<0:break
            hits.append(start+i);i+=1
    print("OPCODE_PATTERN",key.hex(),"count",len(hits))
    for a in hits[:20]:dump(a,35,110)
print("PACKET_AUDIT: STATIC_ONLY; sender ABI not certified by opcode or xrefs alone")
