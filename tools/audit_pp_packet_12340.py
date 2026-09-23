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

# Inspect native spell serialization and client transport entrypoints at instruction-aligned starts.
for name, address, nbytes in [
    ("cast_serializer_candidate", 0x0080CCE0, 1450),
    ("network_send_candidate", 0x0081B530, 950),
    ("native_spell_send_caller", 0x0080DA40, 100),
]:
    for start, buf, section in secs:
        if not start <= address < start + len(buf):
            continue
        print("TARGETED", name, hex(address))
        for ins in md.disasm(buf[address-start:address-start+nbytes], address):
            print("INS",hex(ins.address),ins.mnemonic,ins.op_str)
        break

# Find repeated client-owned CDataStore outbound-destruction call fan-ins.
# This only nominates send ABI candidates; no function is called or hooked.
from collections import Counter
vtable=(0x009E0E24).to_bytes(4,"little")
fanin=Counter()
occurrences=[]
for start,buf,name in secs:
    p=0
    while True:
        p=buf.find(vtable,p)
        if p<0:break
        at=start+p
        prior=buf[max(0,p-100):p]
        directs=[]
        for off in range(len(prior)-4):
            if prior[off]!=0xE8:continue
            rel=struct.unpack_from("<i",prior,off+1)[0]
            callat=start+max(0,p-100)+off
            dest=callat+5+rel
            if base+0x1000<=dest<base+len(exe)+0x100000:
                directs.append((callat,dest))
        if directs:
            last=directs[-1]
            fanin[last[1]]+=1
            occurrences.append((at,last[0],last[1]))
        p+=4
print("DATASTORE_VTABLE_REFERENCES",len(occurrences))
print("DATASTORE_PREDESTRUCTOR_LAST_CALLS",[(hex(k),v) for k,v in fanin.most_common(25)])
for addr,callat,dest in occurrences[:45]:
    print("DATASTORE_EXAMPLE",hex(addr),hex(callat),hex(dest))
for dest,count in fanin.most_common(6):
    if count>=3:dump(dest,0,260)

# Candidate functions from third-party 12340 spellqueue/packet tools are NOT
# assumed compatible with this pinned executable; verify their exact bytes.
for name,addr in [
    ("spell_packet_construct",0x0080B2F5),
    ("spell_packet_send_site",0x0080B4EE),
    ("spellqueue_send_candidate",0x006B0B50),
    ("warden_send_candidate",0x00632B50)
]:
    print("EXTERNAL_SEND_CANDIDATE",name,hex(addr))
    dump(addr,0,280)
for target in (0x006B0B50,0x00632B50):
    found=[]
    for start,buf,name in secs:
        for i in range(len(buf)-4):
            if buf[i]!=0xE8:continue
            rel,=struct.unpack_from("<i",buf,i+1)
            if start+i+5+rel==target:found.append(start+i)
    print("SEND_XREF",hex(target),len(found),[hex(x) for x in found[:32]])

for name,addr,n in [
    ("datastore_init_packet",0x0047B0A0,380),
    ("datastore_read_slice",0x0047B6B0,260),
    ("spell_send_caller",0x0080B4D0,75),
]:
    print("DATASTORE_ABI_CANDIDATE",name,hex(addr))
    dump(addr,0,n)

# Host's executable-byte safety gate MUST match the exact pinned image.
# Previous static audits showed xrefs but did not assert every runtime gate.
checks=[
 (0x006B0B50,"send_head","55 8b ec 8b 0d f4 9c c7 00 85 c9 74 0b 8b 45 08 50 e8"),
 (0x00632B50,"native_head","55 8b ec 56 8b f1 83 be 34 05 00 00 05"),
 (0x0080B4E3,"caller_head","8d 55 e4 52 c7 45 f8 00 00 00 00 e8"),
 (0x0080DA40,"cast_prefix","55 8b ec e8 48 5d cc ff 68 a0 00 00 00 68 40 23 9f 00"),
 (0x00510423,"caller_postfix","83 c4 14"),
 (0x006E6F10,"pos_prefix","55 8b ec"),
 (0x00819210,"lua_exec_head","55 8b ec 51 83 05 a0 13 d4 00 01"),
 (0x00818010,"lua_get_head","55 8b ec 8b 45 08 56 8b 35 8c f7 d3 00 57 50 56"),
 (0x0071F300,"creature_head","80 b9 f4 09 00 00 00 74 04 33 c0 eb 0d 8b 81 d0 00 00 00 0f b6 80 d3 01 00 00"),
 (0x004F7494,"creature_caller","8b ce e8 65 7e 22 00 83 f8 0c"),
]
def static_read(va,size):
    for start,buf,name in secs:
        if start<=va and va+size<=start+len(buf):
            return buf[va-start:va-start+size]
    raise RuntimeError(f"unmapped executable 0x{va:08x}")
all_gates=True
for va,name,hexes in checks:
    expect=bytes.fromhex(hexes)
    actual=static_read(va,len(expect))
    status="PASS" if actual==expect else "FAIL"
    if actual!=expect: all_gates=False
    print("PP_RUNTIME_GATE",status,name,hex(va),"actual",actual.hex(),"expected",expect.hex())
print("PP_RUNTIME_GATE_SUMMARY", "PASS" if all_gates else "FAIL")
if not all_gates: raise SystemExit("PP_STATIC_ABI_RUNTIME_MISMATCH")

# New: trace exact 3.3.5 spell-target layout + network store forwarding.
for name, address, nbytes in [
    ("SpellCastTargets::Write?",0x00809F80,1100),
    ("CDataStorePacketConstructor",0x0047AFA0,220),
    ("CDataStoreByteWriter",0x0047AFE0,210),
    ("NativeSendDatastore",0x00632B50,450),
    ("OriginalCastPacketBuild",0x0080B2F5,650),
    ("SpellInfoClientCastFlags",0x0080B3DB,240),
]:
    print("PP_CANDIDATE_DISASM",name,hex(address))
    for start,buf,section in secs:
        if start<=address<start+len(buf):
            for ins in md.disasm(buf[address-start:address-start+nbytes],address):
                print("PP_INS",hex(ins.address),ins.mnemonic,ins.op_str)
            break
print("PACKET_AUDIT: STATIC_ONLY; sender ABI not certified by opcode or xrefs alone")
