"""Fail-closed, static PE verification of the isolated EpochConnection TEST pair."""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path
from audit_loader_12340 import inspect_imports
from epoch_connection_loader_patch import PINNED_SHA, PINNED_SIZE, patch_epoch

EXPECTED_EXPORTS = {
    1: "EpochConnectionAnchor",
    2: "EpochConnectionCheck",
    3: "EpochConnectionStatus",
}

def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

class PE:
    def __init__(self, data: bytes):
        self.data = data
        if len(data) < 512 or data[:2] != b"MZ":
            raise ValueError("not MZ")
        self.pe = self.u32(0x3c)
        if data[self.pe:self.pe+4] != b"PE\0\0":
            raise ValueError("invalid PE signature")
        if self.u16(self.pe+4) != 0x14c or self.u16(self.pe+24) != 0x10b:
            raise ValueError("not PE32 x86")
        if not self.u16(self.pe+22) & 0x2000:
            raise ValueError("not DLL")
        self.opt = self.pe+24
        self.count = self.u16(self.pe+6)
        self.sec_off = self.opt+self.u16(self.pe+20)
        self.sections = []
        for index in range(self.count):
            off=self.sec_off+index*40
            name=data[off:off+8].rstrip(b"\0").decode("ascii")
            vsize, rva, size, ptr=struct.unpack_from("<IIII",data,off+8)
            if size and (ptr < self.u32(self.opt+60) or ptr+size > len(data)):
                raise ValueError("out-of-range section: "+name)
            self.sections.append((name,vsize,rva,size,ptr))
        if self.u32(self.opt+56) < max(
            (rva+max(vsize,size) for name,vsize,rva,size,ptr in self.sections),
            default=0):
            raise ValueError("SizeOfImage smaller than sections")

    def u16(self, off: int) -> int:
        return struct.unpack_from("<H",self.data,off)[0]

    def u32(self, off: int) -> int:
        return struct.unpack_from("<I",self.data,off)[0]

    def directory(self, index: int) -> tuple[int,int]:
        return self.u32(self.opt+96+index*8), self.u32(self.opt+100+index*8)

    def offset(self, rva: int, length: int=1) -> int:
        for name,vsize,va,size,ptr in self.sections:
            if va <= rva and rva+length <= va+min(vsize or size,size):
                return ptr+rva-va
        if rva+length <= self.u32(self.opt+60):
            return rva
        raise ValueError("unmapped RVA 0x%x" % rva)

    def blob(self,index: int) -> bytes:
        rva,size=self.directory(index)
        return self.data[self.offset(rva,size):self.offset(rva,size)+size] if rva and size else b""

    def ascii(self, rva: int) -> str:
        off=self.offset(rva)
        end=self.data.find(b"\0",off,min(off+512,len(self.data)))
        if end == -1:
            raise ValueError("unterminated export")
        return self.data[off:end].decode("ascii")

    def exports(self) -> dict[int,str]:
        rva,size=self.directory(0)
        if not rva or size<40:
            raise ValueError("missing export directory")
        off=self.offset(rva,40)
        base=self.u32(off+16)
        functions,names=self.u32(off+20),self.u32(off+24)
        function_rva,names_rva,ordinals_rva=(self.u32(off+28),
            self.u32(off+32),self.u32(off+36))
        if functions>4096 or names>4096 or names>functions:
            raise ValueError("export bounds")
        result={}
        for i in range(names):
            name_rva=self.u32(self.offset(names_rva+i*4,4))
            index=self.u16(self.offset(ordinals_rva+i*2,2))
            if index>=functions:
                raise ValueError("invalid export name ordinal")
            ordinal=base+index
            address=self.u32(self.offset(function_rva+index*4,4))
            if not address:
                raise ValueError("null export address")
            result[ordinal]=self.ascii(name_rva)
        return result

def verify(original: bytes, patched: bytes, loader: bytes, branch: str,
           commit: str) -> dict:
    if len(original)!=PINNED_SIZE or sha(original)!=PINNED_SHA:
        raise ValueError("original EpochConnection bytes not pinned source")
    if len(commit)!=40 or any(c not in "0123456789abcdef" for c in commit):
        raise ValueError("invalid Git commit SHA")
    if branch!="feature/loader-12340":
        raise ValueError("wrong branch for isolated TEST pair")
    old, new, ld = PE(original), PE(patched), PE(loader)
    expected,patch_report=patch_epoch(original,PINNED_SHA)
    if patched!=expected:
        raise ValueError("patched Epoch DLL does not match deterministic verified patch")
    if old.exports()!=EXPECTED_EXPORTS or new.exports()!=EXPECTED_EXPORTS:
        raise ValueError("EpochConnection ordinal exports 1-3 changed/missing")
    if ld.exports().get(1)!="Wow335LoaderAnchor":
        raise ValueError("Wow335Loader missing ordinal 1 anchor")
    if new.count!=old.count+1 or new.sections[:-1]!=old.sections:
        raise ValueError("existing section descriptors not preserved")
    if new.sections[-1][0]!=".w335ldr":
        raise ValueError("unexpected added section")
    if new.u32(new.opt+56)<new.sections[-1][2]+new.sections[-1][1]:
        raise ValueError("new section out of SizeOfImage")
    for name,vsize,rva,size,ptr in old.sections:
        if original[ptr:ptr+size]!=patched[ptr:ptr+size]:
            raise ValueError("original section modified: "+name)
    # Preserve every original directory other than relocated import-descriptor array.
    for index in range(16):
        if index==1:
            continue
        if old.directory(index)!=new.directory(index):
            raise ValueError("original data-directory pointer changed: "+str(index))
        if index!=4 and old.blob(index)!=new.blob(index):
            raise ValueError("original data-directory contents changed: "+str(index))
    old_imports=inspect_imports(original)["imported_dlls"]
    new_imports=inspect_imports(patched)["imported_dlls"]
    if new_imports!=old_imports+[{"dll":"Wow335Loader.dll","functions":["#1"]}]:
        raise ValueError("original imports or new loader ordinal import invalid")
    # Loader's startup dependency must be statically resolvable to Windows system DLLs.
    imports=inspect_imports(loader)["imported_dlls"]
    dependency_names=[item["dll"] for item in imports]
    if not dependency_names or any(
        not name.lower().startswith(("kernel32.dll","api-ms-win-","vcruntime",
                                     "ucrtbase.dll","msvcrt.dll","user32.dll"))
        for name in dependency_names):
        raise ValueError("unexpected loader dependency: "+repr(dependency_names))
    if new.directory(9)[0]==0 or new.directory(5)[0]==0:
        raise ValueError("TLS and base relocation must remain present")
    return {
        "schema_version":1,
        "kind":"EPOCH_CONNECTION_ISOLATED_TEST_PAIR",
        "branch":branch,"git_sha":commit,"target_build":12340,"arch":"x86",
        "original_epoch_sha256":sha(original),
        "original_epoch_size":len(original),
        "client_exe_sha256":"2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d",
        "files":{
            "EpochConnection.dll":{"sha256":sha(patched),"size":len(patched)},
            "Wow335Loader.dll":{"sha256":sha(loader),"size":len(loader)},
        },
        "original_exports_preserved":True,
        "original_imports_preserved":True,
        "original_tls_preserved":True,
        "original_relocations_preserved":True,
        "original_sections_preserved":True,
        "startup_import":"Wow335Loader.dll:#1",
        "loader_imports":dependency_names,
        "module_load_order":[],
        "final_package":"NOT_RUN",
        "in_game_verified":False,
        "network_behavior_verified":False,
        "pe_integrity":"PASS",
        "test_pair_gate":"PASS",
    }

def main() -> int:
    parser=argparse.ArgumentParser()
    parser.add_argument("--original",required=True)
    parser.add_argument("--patched",required=True)
    parser.add_argument("--loader",required=True)
    parser.add_argument("--branch",required=True)
    parser.add_argument("--commit",required=True)
    parser.add_argument("--report",required=True)
    args=parser.parse_args()
    try:
        report=verify(*(Path(p).read_bytes() for p in (
            args.original,args.patched,args.loader)),args.branch,args.commit)
        output=Path(args.report)
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8")
        print("EPOCH_TEST_PAIR: PASS",report["git_sha"])
        print("EPOCH_TEST_PAIR_FILES:",report["files"])
        print("FINAL_PACKAGE: NOT_RUN; NETWORK_BEHAVIOR: UNVERIFIED")
        return 0
    except (OSError,ValueError,struct.error,IndexError,KeyError) as exc:
        print("EPOCH_TEST_PAIR: FAIL",exc)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
