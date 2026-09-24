"""Static research for pinned WoW 3.3.5a x86 camera/target candidates.

Reports section mapping and first bytes ONLY. Never treats an upstream
ConsoleXP address as verified ABI, never mutates the game binary.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CANDIDATES=(
    ("object_manager_connection",0x00C79CE0,"335 AutoLoot observer"),
    ("get_object_position",0x006E6F10,"335 AutoLoot host"),
    ("get_active_camera",0x004F5960,"ConsoleXP upstream Game.cpp"),
    ("world_frame_pointer",0x00B7436C,"ConsoleXP upstream Game.cpp"),
    ("world_frame_alternative",0x00EEEA8C,"jrsa 3.3.5a camera study: alternative worldframe global"),
    ("get_object_by_guid",0x004D4DB0,"ConsoleXP upstream Game.h"),
)

def pe_layout(data: bytes):
    if len(data)<512 or data[:2]!=b"MZ":
        raise ValueError("not a Windows EXE")
    pe=struct.unpack_from("<I",data,0x3c)[0]
    if pe+24>len(data) or data[pe:pe+4]!=b"PE\x00\x00":
        raise ValueError("invalid PE header")
    machine,count,opt_size=(
        struct.unpack_from("<H",data,pe+off)[0] for off in (4,6,20))
    opt=pe+24
    if machine!=0x14c or opt+opt_size>len(data) or opt_size<64 or (
        struct.unpack_from("<H",data,opt)[0]!=0x10b):
        raise ValueError("not a 32-bit x86 PE image")
    image_base=struct.unpack_from("<I",data,opt+28)[0]
    headers=struct.unpack_from("<I",data,opt+60)[0]
    result=[]
    for i in range(count):
        pos=opt+opt_size+40*i
        if pos+40>len(data):
            raise ValueError("truncated section table")
        name=data[pos:pos+8].split(b"\0",1)[0].decode("ascii",errors="replace")
        virtual_size,rva,raw_size,raw_ptr=struct.unpack_from("<IIII",data,pos+8)
        flags=struct.unpack_from("<I",data,pos+36)[0]
        if raw_size and raw_ptr+raw_size>len(data):
            raise ValueError("section points outside file")
        result.append((name,rva,virtual_size,raw_ptr,raw_size,flags))
    return image_base,headers,result

def va_info(data: bytes,va: int):
    image_base,headers,sections=pe_layout(data)
    if va<image_base:
        return {"va":f"0x{va:08x}","mapped":False,"reason":"below image base"}
    rva=va-image_base
    if rva<headers:
        offset=rva
        sec="PE_HEADERS"
        executable=False
    else:
        chosen=next((s for s in sections
                    if s[1]<=rva<s[1]+max(s[2],s[4])),None)
        if not chosen:
            return {"va":f"0x{va:08x}","mapped":False,"reason":"unmapped RVA"}
        sec,srva,virtual_size,raw_ptr,raw_size,flags=chosen
        delta=rva-srva
        if delta>=raw_size:
            return {"va":f"0x{va:08x}","mapped":False,
                    "section":sec,"reason":"uninitialized section data"}
        offset=raw_ptr+delta
        executable=bool(flags & 0x20000000)
    return {"va":f"0x{va:08x}","mapped":True,
            "section":sec,"executable_section":executable,
            "file_offset":offset,"first_16_bytes":data[offset:offset+16].hex()}

def audit(exe: Path,manifest: dict):
    raw=exe.read_bytes()
    digest=hashlib.sha256(raw).hexdigest()
    if digest!=manifest.get("sha256") or len(raw)!=manifest.get("size"):
        raise ValueError("EXACT_CLIENT_MISMATCH: selected WoW.exe differs")
    base,headers,sections=pe_layout(raw)
    result=[]
    for name,va,origin in CANDIDATES:
        row=va_info(raw,va)
        row.update({"id":name,"origin":origin,
                    "exact_12340_abi_verified":False})
        result.append(row)
    # Absolute-memory immediates in executable sections are candidate xrefs;
    # finding a reference does not prove a global points at a world frame.
    for row in result:
        if row["id"] not in ("world_frame_pointer","world_frame_alternative","get_active_camera"):
            continue
        needle=struct.pack("<I",int(row["va"],16))
        refs=[]
        for name,rva,virtual_size,raw_ptr,raw_size,flags in sections:
            if not (flags & 0x20000000):
                continue
            section=raw[raw_ptr:raw_ptr+raw_size]
            pos=0
            while len(refs)<20:
                at=section.find(needle,pos)
                if at<0:break
                pos=at+4
                refs.append({"immediate_va":f"0x{base+rva+at:08x}",
                             "context":section[max(0,at-12):at+20].hex()})
        row["executable_immediate_xrefs"]=refs
        row["first_64_bytes"]=(raw[row["file_offset"]:row["file_offset"]+64].hex()
            if row.get("mapped") else None)
    return {"client_sha256":digest,"client_bytes":len(raw),
            "image_base":f"0x{base:08x}",
            "purpose":"static leads only; no function signatures/ABIs proven",
            "candidates":result}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--exe",type=Path,default=ROOT/"Wow.exe")
    p.add_argument("--manifest",type=Path,default=ROOT/"runtime/client_exe_target.json")
    p.add_argument("--report",type=Path,default=ROOT/"dist/esp_client_static_audit.json")
    args=p.parse_args()
    data=audit(args.exe,json.loads(args.manifest.read_text(encoding="utf-8")))
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(data,indent=2)+"\n",encoding="utf-8")
    print("ESP_CLIENT_STATIC_AUDIT: PASS; exact SHA; ABI still unverified")
    for candidate in data["candidates"]:
        print(candidate["id"],candidate["va"],candidate.get("section"),
              "mapped",candidate["mapped"],"code",candidate.get("executable_section"))

if __name__=="__main__":
    main()
