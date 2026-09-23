"""Exact-pinned-client static evidence for the native CGUnit creature-type ABI."""
from __future__ import annotations
import hashlib
import json
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
TYPE_VA=0x0071F300
CALLER_VA=0x004F7494
TYPE_PREFIX=bytes.fromhex(
    "80 b9 f4 09 00 00 00 74 04 33 c0 eb 0d "
    "8b 81 d0 00 00 00 0f b6 80 d3 01 00 00"
)
CALLER=bytes.fromhex("8b ce e8 65 7e 22 00 83 f8 0c")

def check_binary(binary: bytes, sha: str, size: int) -> None:
    if len(binary)!=size or hashlib.sha256(binary).hexdigest()!=sha:
        raise ValueError("wrong exact Wow.exe hash or size")
    if binary[:2]!=b"MZ": raise ValueError("not MZ")
    pe=struct.unpack_from("<I",binary,0x3c)[0]
    if binary[pe:pe+4]!=bytes((80,69,0,0)) or struct.unpack_from("<H",binary,pe+4)[0]!=0x14c:
        raise ValueError("not PE32 x86")
    opt=pe+24
    if struct.unpack_from("<H",binary,opt)[0]!=0x10b: raise ValueError("not PE32")
    base=struct.unpack_from("<I",binary,opt+28)[0]
    n=struct.unpack_from("<H",binary,pe+6)[0]
    headers=opt+struct.unpack_from("<H",binary,pe+20)[0]
    def va_bytes(va:int,length:int)->bytes:
        for i in range(n):
            off=headers+40*i
            vs,rva,raw_size,raw=struct.unpack_from("<IIII",binary,off+8)
            flags=struct.unpack_from("<I",binary,off+36)[0]
            if (flags&0x20000000) and base+rva<=va and va+length<=base+rva+min(vs,raw_size):
                at=raw+va-(base+rva)
                return binary[at:at+length]
        raise ValueError(f"unmapped executable VA 0x{va:08x}")
    if va_bytes(TYPE_VA,len(TYPE_PREFIX))!=TYPE_PREFIX:
        raise ValueError("native creature type function bytes changed")
    if va_bytes(CALLER_VA,len(CALLER))!=CALLER:
        raise ValueError("native caller ECX/EAX and E8 displacement changed")
    disp=struct.unpack_from("<i",CALLER,3)[0]
    if CALLER_VA+2+5+disp!=TYPE_VA:
        raise ValueError("call does not resolve to creature type function")
    print(f"PP_NPC_TYPE_AUDIT: PASS pinned sha256={sha} VA=0x{TYPE_VA:08x} caller=0x{CALLER_VA+2:08x} candidate ABI=thiscall ECX -> EAX")

def main()->int:
    manifest=json.loads((ROOT/"runtime/client_exe_target.json").read_text(encoding="utf-8"))
    try: check_binary((ROOT/"Wow.exe").read_bytes(),manifest["sha256"],manifest["size"])
    except (OSError,ValueError,KeyError,struct.error) as error:
        print("PP_NPC_TYPE_AUDIT: FAIL",error)
        return 1
    return 0

if __name__=="__main__":
    raise SystemExit(main())
