"""Read-only structural PE32 audit of the exact pinned 12340 Wow.exe.

Reports occupied header bytes, original directory mappings and raw-file pointers.
Never rewrites the client. A mapped directory does not establish game compatibility.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path
from verify_client_exe import inspect_exe, verify_target

DIR_NAMES = ("export", "import", "resource", "exception", "security",
             "base_reloc", "debug", "architecture", "global_ptr", "tls",
             "load_config", "bound_import", "iat", "delay_import", "clr", "reserved")


def audit_layout(data: bytes) -> dict:
    def rd16(off):
        if off < 0 or off + 2 > len(data): raise ValueError("truncated u16")
        return struct.unpack_from("<H", data, off)[0]

    def rd32(off):
        if off < 0 or off + 4 > len(data): raise ValueError("truncated u32")
        return struct.unpack_from("<I", data, off)[0]

    def hx(x): return "0x%x" % x

    if len(data) < 512 or data[:2] != b"MZ":
        raise ValueError("not MZ")
    pe = rd32(0x3c)
    if data[pe:pe+4] != b"PE\0\0" or rd16(pe+4) != 0x14c:
        raise ValueError("not PE i386")
    n, optsz = rd16(pe+6), rd16(pe+20)
    opt = pe+24
    if rd16(opt) != 0x10b or not 0 < n <= 96 or optsz < 224:
        raise ValueError("invalid PE32 section count or optional header")
    table = opt+optsz
    end = table+40*n
    if end > len(data): raise ValueError("section table exceeds file")
    header_size = rd32(opt+60)
    image_size = rd32(opt+56)
    file_align = rd32(opt+36)
    if file_align < 1 or file_align & (file_align-1):
        raise ValueError("invalid FileAlignment")
    rows = []
    first_raw = len(data)
    last_raw = 0
    for i in range(n):
        off=table+40*i
        raw_name = data[off:off+8]
        vsize, va, rawsz, rawoff = struct.unpack_from("<IIII",data,off+8)
        relocs, linenums = rd32(off+24), rd32(off+28)
        if rawsz:
            if rawoff + rawsz > len(data):
                raise ValueError("section beyond EOF")
            first_raw = min(first_raw, rawoff)
            last_raw = max(last_raw, rawoff+rawsz)
        rows.append({"index":i,"header_offset":hx(off),
                     "name":raw_name.rstrip(b"\0").decode("ascii","backslashreplace"),
                     "virtual_size":hx(vsize),"rva":hx(va),
                     "raw_size":hx(rawsz),"raw_offset":hx(rawoff),
                     "relocations_file_pointer":hx(relocs),
                     "line_numbers_file_pointer":hx(linenums)})
    if first_raw < end or header_size > first_raw:
        raise ValueError("section headers/raw data overlap")
    tail = data[end:first_raw]
    occupied = [i for i,b in enumerate(tail) if b]
    segments=[]
    for i in range(0,len(tail),16):
        block=tail[i:i+16]
        if any(block):
            segments.append({"offset":hx(end+i),"hex":block.hex(),
                             "printable":"".join(chr(ch) if 32<=ch<127 else "." for ch in block)})
    def map_rva(rva,length):
        if not rva or length<0 or rva+length>0x100000000: return None
        if rva<header_size and rva+length<=header_size and rva+length<=len(data):
            return rva
        for row in rows:
            va=int(row["rva"],16)
            rawsz=int(row["raw_size"],16)
            rawoff=int(row["raw_offset"],16)
            vsize=int(row["virtual_size"],16)
            if va<=rva and rva+length<=va+min(vsize or rawsz,rawsz):
                off=rawoff+rva-va
                if off+length<=len(data): return off
        return None
    directories=[]
    numdirs=rd32(opt+92)
    for i in range(min(numdirs,16)):
        rva,size=rd32(opt+96+8*i),rd32(opt+100+8*i)
        entry={"name":DIR_NAMES[i],"address":hx(rva),"size":size}
        if rva or size:
            entry["address_kind"]="file_offset" if i==4 else "rva"
            mapped=rva if i==4 else map_rva(rva,size)
            entry["mapped_raw_offset"]=hx(mapped) if mapped is not None else None
            entry["bounds_valid"]=bool(rva and size and mapped is not None and
                                       mapped+size<=len(data))
            if i==4:
                entry["would_require_relocation_if_raw_section_inserted"]=rva>=first_raw
        directories.append(entry)
    debug_records=[]
    if len(directories)>6 and directories[6]["size"]:
        debug=directories[6]
        p=map_rva(int(debug["address"],16),debug["size"])
        if p is None or debug["size"]%28:
            raise ValueError("unmappable/malformed debug directory")
        for i in range(debug["size"]//28):
            base=p+28*i
            raw_ptr=rd32(base+24)
            debug_records.append({"entry":i,"header_raw_offset":hx(base),
                                  "pointer_to_raw_data":hx(raw_ptr),
                                  "size_of_data":rd32(base+16),
                                  "pointer_inside_file":raw_ptr<len(data)})
    tls_fields = None
    if len(directories)>9 and directories[9]["size"]:
        addr=int(directories[9]["address"],16)
        off=map_rva(addr,24)
        if off is None: raise ValueError("unmappable TLS directory")
        names=("StartAddressOfRawData","EndAddressOfRawData",
               "AddressOfIndex","AddressOfCallbacks","SizeOfZeroFill","Characteristics")
        tls_fields={name:hx(rd32(off+i*4)) for i,name in enumerate(names)}
    return {
        "pe_offset":hx(pe),"coff_section_count":n,
        "coff_symbol_table_file_pointer":hx(rd32(pe+12)),
        "coff_symbol_count":rd32(pe+16),"optional_header_size":hx(optsz),
        "optional_header_offset":hx(opt),"section_table_start":hx(table),
        "section_table_end":hx(end),"size_of_headers":hx(header_size),
        "first_section_raw":hx(first_raw),"end_of_last_raw_section":hx(last_raw),
        "file_alignment":hx(file_align),"section_alignment":hx(rd32(opt+32)),
        "address_of_entry_point_rva":hx(rd32(opt+16)),
        "image_base":hx(rd32(opt+28)),"size_of_image":hx(image_size),
        "size_of_code":hx(rd32(opt+4)),"size_of_initialized_data":hx(rd32(opt+8)),
        "checksum":hx(rd32(opt+64)),"data_directory_count":numdirs,
        "sections":rows,"data_directories":directories,
        "debug_raw_pointer_records":debug_records,"tls_fields":tls_fields,
        "header_tail_nonzero":bool(occupied),
        "header_tail_nonzero_count":len(occupied),
        "header_tail_first_nonzero":hx(end+occupied[0]) if occupied else None,
        "header_tail_16_byte_blocks":segments,
        "header_tail_recognizable_names":[name for name in
            (".text",".rdata",".data",".zdata",".tls",".rsrc",".detour")
            if name.encode() in tail],
        "overlay_bytes_after_last_raw_section":len(data)-last_raw,
        "read_only":True,
        "note":"RVA/VA differ from raw file offsets. Occupied tail bytes are not an unused section-header slot."
    }


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--exe",default="Wow.exe")
    ap.add_argument("--target",default="runtime/client_exe_target.json")
    ap.add_argument("--report",default="dist/pe_layout_12340.json")
    args=ap.parse_args()
    try:
        path=Path(args.exe)
        target=json.loads(Path(args.target).read_text(encoding="utf-8"))
        verify_target(inspect_exe(path),target)
        report=audit_layout(path.read_bytes())
        report["source_sha256"]=hashlib.sha256(path.read_bytes()).hexdigest()
        report["target_build"]=12340
        out=Path(args.report)
        out.parent.mkdir(parents=True,exist_ok=True)
        out.write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8")
        print("PE_LAYOUT_12340: PASS; exact pinned client; read-only")
        print("PE_LAYOUT_12340_REPORT:",json.dumps(report,sort_keys=True,separators=(",",":")))
        return 0
    except (OSError,ValueError,KeyError,struct.error,TypeError) as exc:
        print("PE_LAYOUT_12340: FAIL",exc)
        return 1


if __name__=="__main__":
    raise SystemExit(main())
