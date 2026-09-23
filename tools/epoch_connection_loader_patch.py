"""Isolated copy-only import experiment for the exact user-provided EpochConnection.dll.

Adds a startup import of Wow335Loader.dll ordinal 1 to an additional PE section.
Never modifies the input binary, the canonical Wow.exe or the game installation.
Does not claim in-game success or produce an updater-ready package.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path
from audit_loader_12340 import inspect_imports

PINNED_SHA = "9af04f7afd21b0bc93860d66e6ccdc96ccf1b039ea31871119a3deaa271352c3"
PINNED_SIZE = 160768
SECTION = b".w335ldr"
DLL_NAME = b"Wow335Loader.dll\0"


def align(value: int, unit: int) -> int:
    if unit < 1 or unit & (unit-1):
        raise ValueError("invalid alignment")
    return (value+unit-1) & ~(unit-1)


def patch_epoch(data: bytes, expected_sha: str) -> tuple[bytes, dict]:
    old_sha = hashlib.sha256(data).hexdigest()
    if old_sha != expected_sha:
        raise ValueError("source EpochConnection.dll SHA256 mismatch")
    if len(data) < 1024 or data[:2] != b"MZ":
        raise ValueError("truncated source DLL")

    old = inspect_imports(data)
    if any(item["dll"].casefold() == DLL_NAME[:-1].decode("ascii").casefold()
           for item in old["imported_dlls"]):
        raise ValueError("source already imports Wow335Loader.dll")
    out = bytearray(data)

    def get16(off):
        if off < 0 or off+2 > len(out):
            raise ValueError("invalid PE u16 offset")
        return struct.unpack_from("<H",out,off)[0]

    def get32(off):
        if off < 0 or off+4 > len(out):
            raise ValueError("invalid PE u32 offset")
        return struct.unpack_from("<I",out,off)[0]

    def put16(off,value): struct.pack_into("<H",out,off,value)
    def put32(off,value): struct.pack_into("<I",out,off,value)

    pe = get32(0x3c)
    if out[pe:pe+4] != b"PE\0\0" or get16(pe+4) != 0x14c:
        raise ValueError("source DLL not x86 PE")
    count, opts = get16(pe+6), get16(pe+20)
    opt = pe+24
    if get16(opt) != 0x10b or opts < 224 or not 0 < count < 96:
        raise ValueError("unsupported PE32 optional header")
    if not get16(pe+22)&0x2000:
        raise ValueError("source not marked as a DLL")
    n_dirs = get32(opt+92)
    if n_dirs < 13:
        raise ValueError("missing export/import/TLS/IAT directories")
    section_align, file_align = get32(opt+32), get32(opt+36)
    size_headers, size_image = get32(opt+60), get32(opt+56)
    section_head = opt+opts+40*count
    first_raw = len(out)
    highest_end = align(size_headers, section_align)
    rows=[]
    for i in range(count):
        off=opt+opts+i*40
        name=bytes(out[off:off+8]).rstrip(b"\0")
        vsize,va,rawsize,rawptr=struct.unpack_from("<IIII",out,off+8)
        if name == SECTION:
            raise ValueError("already patched section")
        if rawsize:
            if rawptr < size_headers or rawptr+rawsize > len(out):
                raise ValueError("invalid existing section bounds")
            first_raw=min(first_raw,rawptr)
        highest_end=max(highest_end,align(va+max(vsize,rawsize),section_align))
        rows.append((va,vsize,rawsize,rawptr))
    if size_headers > first_raw or section_head+40 > first_raw or any(out[section_head:section_head+40]):
        raise ValueError("no vacant section-header slot in EpochConnection.dll")
    if highest_end > size_image:
        raise ValueError("inconsistent SizeOfImage")
    if get32(opt+96+4*8) or get32(opt+96+4*8+4):
        raise ValueError("security directory present; refusing to invalidate signed DLL")
    if get32(opt+96+11*8) or get32(opt+96+11*8+4):
        raise ValueError("bound imports present")
    # This exact reference file has no overlay. Refuse to move unclassified appended data.
    raw_end=max(rawptr+rawsize for va,vsize,rawsize,rawptr in rows if rawsize)
    if raw_end != len(data):
        raise ValueError("unexpected overlay; refusing to change its placement")
    import_rva, import_size = get32(opt+104), get32(opt+108)
    export_rva, export_size = get32(opt+96), get32(opt+100)
    tls_rva,tls_size=get32(opt+96+9*8),get32(opt+100+9*8)
    reloc_rva,reloc_size=get32(opt+96+5*8),get32(opt+100+5*8)
    debug_rva,debug_size=get32(opt+96+6*8),get32(opt+100+6*8)

    def rva_offset(rva,length=1):
        if rva < size_headers and rva+length <= size_headers and rva+length<=len(data):
            return rva
        for va,vsize,rawsize,rawptr in rows:
            if va<=rva and rva+length<=va+min(vsize or rawsize,rawsize):
                off=rawptr+rva-va
                if off+length<=len(data): return off
        raise ValueError("unmapped original RVA: 0x%x" % rva)

    if not 20 <= import_size <= 0x100000 or not import_rva or not export_rva or export_size<40:
        raise ValueError("invalid import/export directories")
    if not tls_rva or tls_size<24 or not reloc_rva or not reloc_size:
        raise ValueError("cannot certify preservation of TLS or base relocations")
    # Immutable snapshots of existing directory data and of all old section contents.
    preserved = {
        "export":data[rva_offset(export_rva,export_size):rva_offset(export_rva,export_size)+export_size],
        "tls":data[rva_offset(tls_rva,tls_size):rva_offset(tls_rva,tls_size)+tls_size],
        "reloc":data[rva_offset(reloc_rva,reloc_size):rva_offset(reloc_rva,reloc_size)+reloc_size],
    }
    if debug_rva and debug_size:
        preserved["debug"]=data[rva_offset(debug_rva,debug_size):rva_offset(debug_rva,debug_size)+debug_size]
    original_sections=[bytes(data[ptr:ptr+size]) for va,vsize,size,ptr in rows if size]

    descriptors=[]
    for i in range(min(4096,import_size//20)):
        off=rva_offset(import_rva+i*20,20)
        desc=bytes(data[off:off+20])
        if desc==bytes(20): break
        descriptors.append(desc)
    else:
        raise ValueError("unterminated original import descriptors")
    if len(descriptors)!=len(old["imported_dlls"]):
        raise ValueError("import descriptor count mismatch")

    new_va=highest_end
    descriptor_size=(len(descriptors)+2)*20
    name_off=align(descriptor_size,4)
    ilt_off=align(name_off+len(DLL_NAME),4)
    iat_off=align(ilt_off+8,4)
    section_size=iat_off+8
    new_raw=align(len(out),file_align)
    rawsize=align(section_size,file_align)
    if new_va+align(section_size,section_align)>0xffffffff:
        raise ValueError("PE image overflow")
    payload=bytearray(rawsize)
    for i,desc in enumerate(descriptors):
        payload[i*20:i*20+20]=desc
    struct.pack_into("<IIIII",payload,len(descriptors)*20,new_va+ilt_off,
                     0,0,new_va+name_off,new_va+iat_off)
    payload[name_off:name_off+len(DLL_NAME)]=DLL_NAME
    struct.pack_into("<II",payload,ilt_off,0x80000001,0)
    struct.pack_into("<II",payload,iat_off,0x80000001,0)
    header=struct.pack("<8sIIIIIIHHI",SECTION,section_size,new_va,rawsize,
                       new_raw,0,0,0,0,0xc0000040)
    out[section_head:section_head+40]=header
    put16(pe+6,count+1)
    put32(opt+56,new_va+align(section_size,section_align))
    put32(opt+8,get32(opt+8)+rawsize)
    put32(opt+64,0)
    put32(opt+104,new_va)
    put32(opt+108,descriptor_size)
    out.extend(bytes(new_raw-len(out)))
    out.extend(payload)
    built=bytes(out)
    reread=inspect_imports(built)
    if reread["imported_dlls"] != old["imported_dlls"] + [
            {"dll":"Wow335Loader.dll","functions":["#1"]}]:
        raise ValueError("original imports not preserved")
    if [built[ptr:ptr+size] for va,vsize,size,ptr in rows if size] != original_sections:
        raise ValueError("existing section bytes changed")
    for name, snapshot in preserved.items():
        if name=="export": rva, size=export_rva,export_size
        elif name=="tls": rva,size=tls_rva,tls_size
        elif name=="reloc": rva,size=reloc_rva,reloc_size
        else: rva,size=debug_rva,debug_size
        off=rva_offset(rva,size)
        if built[off:off+size] != snapshot:
            raise ValueError(name+" directory changed")
    return built, {
        "kind":"EPOCH_STARTUP_IMPORT_COPY_ONLY_NOT_GAME_PACKAGE",
        "original_sha256":old_sha,"patched_sha256":hashlib.sha256(built).hexdigest(),
        "original_size":len(data),"patched_size":len(built),
        "original_exports_preserved":True,"original_imports_preserved":True,
        "original_tls_preserved":True,"original_relocations_preserved":True,
        "original_sections_preserved":True,"new_import":"Wow335Loader.dll",
        "new_import_ordinal":1,"new_section":SECTION.decode("ascii"),
        "game_client_version":"3.3.5.12340","architecture":"x86",
        "in_game_verified":False,"final_package":"NOT_RUN",
        "note":"Static byte-preservation checks do not establish correct runtime network behavior."
    }


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True)
    ap.add_argument("--output",default="dist/epoch_preview/EpochConnection.dll")
    ap.add_argument("--report",default="dist/epoch_preview/epoch_patch.json")
    args=ap.parse_args()
    try:
        src=Path(args.input)
        payload=src.read_bytes()
        if len(payload)!=PINNED_SIZE or hashlib.sha256(payload).hexdigest()!=PINNED_SHA:
            raise ValueError("not exact user-supplied reference EpochConnection.dll")
        dest=Path(args.output)
        if src.resolve()==dest.resolve():
            raise ValueError("input and output must not be the same file")
        patched, report=patch_epoch(payload,PINNED_SHA)
        dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(patched)
        path=Path(args.report)
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8")
        print("EPOCH_IMPORT_PREVIEW: PASS",report["patched_sha256"])
        print("NOT_GAME_PACKAGE: integration and game test still required")
        return 0
    except (OSError,ValueError,struct.error,UnicodeError,KeyError,TypeError) as exc:
        print("EPOCH_IMPORT_PREVIEW: FAIL",exc)
        return 1


if __name__=="__main__":
    raise SystemExit(main())
