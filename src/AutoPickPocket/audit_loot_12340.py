"""Read-only exact-12340 loot transaction ABI survey; never patches or runs Wow.exe.

WoW112 sends CMSG_LOOT_MONEY and CMSG_LOOT_RELEASE through a hooked
5875 DataStore. TrinityCore's 3.3.5 protocol also assigns opcodes
0x15E/0x15F, but this does NOT certify the 12340 client's internal
send callsites, incoming dispatch ABI, or release lifecycle.
"""
from pathlib import Path
import hashlib
import json
import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT=Path(__file__).resolve().parents[2]
data=(ROOT/"Wow.exe").read_bytes()
pin=json.loads((ROOT/"runtime/client_exe_target.json").read_text())
if hashlib.sha256(data).hexdigest()!=pin["sha256"] or len(data)!=pin["size"]:
    raise SystemExit("PP_LOOT_AUDIT: REJECT wrong exact Wow.exe")
pe=struct.unpack_from("<I",data,0x3c)[0]
if data[:2]!=b"MZ" or data[pe:pe+4]!=bytes((80,69,0,0)):
    raise SystemExit("PP_LOOT_AUDIT: REJECT non-PE")
opt=pe+24
if struct.unpack_from("<H",data,pe+4)[0]!=0x14c or struct.unpack_from("<H",data,opt)[0]!=0x10b:
    raise SystemExit("PP_LOOT_AUDIT: REJECT non-PE32 x86")
base=struct.unpack_from("<I",data,opt+28)[0]
sections=[]
for i in range(struct.unpack_from("<H",data,pe+6)[0]):
    h=opt+struct.unpack_from("<H",data,pe+20)[0]+40*i
    n,rva,rawsize,raw=struct.unpack_from("<IIII",data,h+8)
    if struct.unpack_from("<I",data,h+36)[0]&0x20000000:
        sections.append((base+rva,data[raw:raw+min(n,rawsize)]))
md=Cs(CS_ARCH_X86,CS_MODE_32)
def window(addr,before=90,after=85):
    for start,buf in sections:
        if start<=addr<start+len(buf):
            at=max(start,addr-before);end=min(start+len(buf),addr+after)
            chunk=buf[at-start:end-start]
            print("PE_WINDOW",hex(addr),"start",hex(at),"raw",chunk.hex())
            print("PE_DISASM",[(hex(x.address),x.mnemonic,x.op_str)
                               for x in list(md.disasm(chunk,at))[:55]])
            return
    print("PE_UNMAPPED",hex(addr))
print("PP_LOOT_AUDIT_PINNED",pin["sha256"],"PE32_X86")
send=0x006B0B50
hits=[]
for start,buf in sections:
    for i in range(len(buf)-4):
        if buf[i]!=0xe8:continue
        rel=struct.unpack_from("<i",buf,i+1)[0]
        if start+i+5+rel==send:hits.append((start+i,start,buf,i))
print("PP_LOOT_SEND_XREF_COUNT",len(hits))
for op,name in [(0x15e,"CMSG_LOOT_MONEY"),(0x15f,"CMSG_LOOT_RELEASE"),
                (0x160,"SMSG_LOOT_RESPONSE"),(0x161,"SMSG_LOOT_RELEASE_RESPONSE")]:
    pattern=struct.pack("<I",op)
    occurrences=[]
    for start,buf in sections:
        at=0
        while True:
            at=buf.find(pattern,at)
            if at<0:break
            occurrences.append(start+at)
            at+=1
    print("PP_LOOT_OPCODE_IMMEDIATE",name,hex(op),"count",len(occurrences),
          "locations",[hex(a) for a in occurrences[:40]])
    for addr in occurrences[:12]:window(addr,36,52)
    if op in (0x15e,0x15f):
        near=[]
        for at,start,buf,off in hits:
            left=max(0,off-320)
            if pattern in buf[left:off]:
                near.append(at)
        print("PP_LOOT_SEND_XREF_WITH_OPCODE",name,
              [hex(x) for x in near[:45]])
        for addr in near[:12]:window(addr,95,80)
source=struct.pack("<I",0x00BFA8D8)
references=[]
for start,buf in sections:
    off=0
    while True:
        off=buf.find(source,off)
        if off<0:break
        references.append(start+off)
        off+=1
print("PP_LOOT_SOURCE_NATIVE_REFERENCES",
      [hex(x) for x in references[:70]],"count",len(references))
for addr in references[:18]:window(addr,35,70)
print("PP_LOOT_AUDIT: STATIC_EVIDENCE_ONLY; NO INBOUND DISPATCH HOOK VERIFIED")
