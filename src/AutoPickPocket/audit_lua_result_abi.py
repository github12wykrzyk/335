"""Static-only exact-pinned PE32 x86 inspection for native Lua policy and result-event ABI.

Print direct E8 callsites and first bytes; do NOT treat executable addresses,
prologues or public offset lists alone as proof of a callable ABI.
"""
from __future__ import annotations
import hashlib
import json
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
VA_NAMES={
  0x00817F90:"FrameScript_RegisterFunction",
  0x00819210:"FrameScript_Execute",
  0x0084E0E0:"Lua_tolstring",
  0x0084E070:"Lua_tointeger",
  0x0084DBD0:"Lua_gettop",
  0x0084DBF0:"Lua_settop",
  0x00818010:"FrameScript_GetGlobalString",
  0x00D3F78C:"Lua_state_global",
}

def audit(data:bytes):
    pe=struct.unpack_from("<I",data,0x3c)[0]
    if data[:2]!=b"MZ" or data[pe:pe+4]!=bytes((80,69,0,0)):
        raise ValueError("bad PE signature")
    if struct.unpack_from("<H",data,pe+4)[0]!=0x14c:
        raise ValueError("bad PE architecture")
    opt=pe+24
    if struct.unpack_from("<H",data,opt)[0]!=0x10b:raise ValueError("bad PE32 magic")
    base=struct.unpack_from("<I",data,opt+28)[0]
    sections=[]
    pos=opt+struct.unpack_from("<H",data,pe+20)[0]
    for n in range(struct.unpack_from("<H",data,pe+6)[0]):
        h=pos+40*n
        size,va,raw_size,raw=struct.unpack_from("<IIII",data,h+8)
        flags=struct.unpack_from("<I",data,h+36)[0]
        sections.append((base+va,size,raw_size,raw,flags))
    for candidate,name in VA_NAMES.items():
        matches=[(st,vs,rawsz,off,flags) for st,vs,rawsz,off,flags in sections
                 if st<=candidate<st+min(vs,rawsz)]
        if len(matches)!=1:
            print(f"PP_LUA_CANDIDATE_UNMAPPED {name} 0x{candidate:08X}")
            continue
        st,vs,rawsz,off,flags=matches[0]
        raw=off+candidate-st
        print(f"PP_LUA_SYMBOL {name} 0x{candidate:08X} exec={bool(flags & 0x20000000)} "
              f"first96={data[raw:raw+96].hex()}")
        if not (flags & 0x20000000):continue
        calls=[]
        for s,vs,raw_size,o,f in sections:
            if not (f&0x20000000):continue
            buf=data[o:o+raw_size]
            for i in range(len(buf)-4):
                if buf[i]==0xE8 and s+i+5+struct.unpack_from("<i",buf,i+1)[0]==candidate:
                    calls.append((s+i,buf[max(0,i-18):i+18].hex()))
        print(f"PP_LUA_XREF_COUNT {name} {len(calls)}")
        for va,context in calls[:14]:
            print(f"PP_LUA_XREF {name} at=0x{va:08X} bytes={context}")
    print("PP_LUA_AUDIT: STATIC_ONLY; no Lua/game callback invoked")

def main():
    target=json.loads((ROOT/"runtime/client_exe_target.json").read_text())
    data=(ROOT/"Wow.exe").read_bytes()
    if hashlib.sha256(data).hexdigest()!=target["sha256"] or len(data)!=target["size"]:
        raise SystemExit("PP_LUA_AUDIT: FAIL wrong exact Wow.exe")
    audit(data)
if __name__=="__main__":
    main()
