"""Build a separate, inspectable 12340 TEST exe with one additional loader import.

This experiment never changes the canonical Wow.exe and is not a playable package.
No 1.12 offsets. Original imported libraries, thunks and EpochConnection import
are preserved as exact bytes; only a *new* import descriptor is appended.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path
from audit_loader_12340 import inspect_imports
from verify_client_exe import inspect_exe, verify_target

NAME = b"Wow335Loader.dll\0"
SECTION = b".w335ldr"
I386 = 0x14c


def rva_offset_existing(source: bytes, table: int, count: int,
                        size_headers: int, rva: int, size: int) -> int:
    if not rva or size < 0 or rva + size > 0xffffffff:
        raise ValueError("invalid debug RVA")
    if rva < size_headers and rva + size <= size_headers:
        return rva
    for i in range(count):
        row = table + i*40
        vsize, va, raw_size, raw_off = struct.unpack_from("<IIII", source, row+8)
        if va <= rva and rva + size <= va + min(vsize or raw_size, raw_size):
            off = raw_off + rva - va
            if off+size <= len(source):
                return off
    raise ValueError("unmapped original debug directory RVA")


def patch(data: bytes, expected_sha256: str) -> tuple[bytes, dict]:
    if hashlib.sha256(data).hexdigest() != expected_sha256:
        raise ValueError("exact pinned 12340 exe SHA256 mismatch")
    original = inspect_imports(data)
    if original["legacy_loader_imported"] or any(
            x["dll"].casefold() == "wow335loader.dll" for x in original["imported_dlls"]):
        raise ValueError("client already contains a startup loader import")

    source = bytearray(data)

    def rd16(offset: int) -> int:
        if offset < 0 or offset + 2 > len(source):
            raise ValueError("truncated PE u16")
        return struct.unpack_from("<H", source, offset)[0]

    def rd32(offset: int) -> int:
        if offset < 0 or offset + 4 > len(source):
            raise ValueError("truncated PE u32")
        return struct.unpack_from("<I", source, offset)[0]

    def wr16(offset: int, value: int):
        struct.pack_into("<H", source, offset, value)

    def wr32(offset: int, value: int):
        struct.pack_into("<I", source, offset, value)

    def align(value: int, unit: int) -> int:
        if unit < 1 or unit & (unit - 1):
            raise ValueError("invalid PE alignment")
        return (value + unit - 1) & ~(unit - 1)

    pe = rd32(0x3c)
    if source[pe:pe+4] != b"PE\0\0" or rd16(pe+4) != I386:
        raise ValueError("not PE32 i386")
    count, opt_size = rd16(pe+6), rd16(pe+20)
    opt = pe + 24
    if rd16(opt) != 0x10b or opt_size < 224 or not 0 < count <= 94:
        raise ValueError("unsupported PE optional header or section count")
    section_align, file_align = rd32(opt+32), rd32(opt+36)
    size_headers, size_image = rd32(opt+60), rd32(opt+56)
    table = opt + opt_size
    section_head = table + count*40
    original_first_raw = len(source)
    highest_va_end = align(size_headers, section_align)
    for i in range(count):
        row = table + i*40
        virtual_size, va, raw_size, raw_off = (
            rd32(row+8), rd32(row+12), rd32(row+16), rd32(row+20))
        if source[row:row+8].rstrip(b"\0") == SECTION:
            raise ValueError("loader PE section already exists")
        if raw_size:
            if raw_off < size_headers or raw_off + raw_size > len(source):
                raise ValueError("invalid existing section raw bounds")
            original_first_raw = min(original_first_raw, raw_off)
        highest_va_end = max(highest_va_end, align(va + max(virtual_size, raw_size), section_align))
    if size_headers > original_first_raw or original_first_raw % file_align:
        raise ValueError("invalid raw section/header layout for safe expansion")
    occupied = source[section_head:min(section_head+40, original_first_raw)]
    if any(occupied):
        first_used = section_head + next(i for i, ch in enumerate(occupied) if ch)
        section_rows = []
        for i in range(count):
            row = table+i*40
            section_rows.append({
                "index": i, "name_hex": source[row:row+8].hex(),
                "virtual_size": hex(rd32(row+8)), "rva": hex(rd32(row+12)),
                "raw_size": hex(rd32(row+16)), "raw_offset": hex(rd32(row+20)),
            })
        raise ValueError(
            "occupied bytes in existing PE headers; refusing overwrite: "
            "section_count=%d optional_header_size=0x%x section_header_offset=0x%x "
            "next_section_header_end=0x%x first_raw=0x%x size_headers=0x%x "
            "occupied_at=0x%x header_tail_hex=%s sections=%s" % (
                count, opt_size, section_head, section_head+40,
                original_first_raw, size_headers, first_used,
                source[section_head:original_first_raw].hex(),
                json.dumps(section_rows, separators=(",", ":"))))
    required_headers = align(max(size_headers, section_head + 40), file_align)
    header_delta = align(max(0, required_headers - original_first_raw), file_align)
    if header_delta:
        # Insert a full file-aligned gap before the first raw section, preserving
        # all existing raw section contents and any trailing overlay byte-for-byte.
        snapshots = [
            (rd32(table+i*40+20), bytes(source[
                rd32(table+i*40+20):rd32(table+i*40+20)+rd32(table+i*40+16)]))
            for i in range(count) if rd32(table+i*40+16)
        ]
        source[original_first_raw:original_first_raw] = bytes(header_delta)
        def adjust_file_pointer(pos: int):
            value = rd32(pos)
            if value >= original_first_raw:
                wr32(pos, value + header_delta)
            elif value and value + 1 > original_first_raw:
                raise ValueError("raw pointer overlaps header insertion")
        adjust_file_pointer(pe + 12)  # COFF symbol table (if any)
        for i in range(count):
            row = table+i*40
            if rd32(row+16):
                adjust_file_pointer(row+20)  # PointerToRawData
            adjust_file_pointer(row+24)  # PointerToRelocations
            adjust_file_pointer(row+28)  # PointerToLinenumbers
        for old_off, payload in snapshots:
            if source[old_off + header_delta:old_off + header_delta + len(payload)] != payload:
                raise ValueError("PE section bytes changed during header relocation")
        # IMAGE_DEBUG_DIRECTORY.PointerToRawData is a file offset, not an RVA.
        debug_rva, debug_size = rd32(opt+96+6*8), rd32(opt+96+6*8+4)
        if debug_rva or debug_size:
            if not debug_rva or debug_size % 28 or debug_size > 28*1024:
                raise ValueError("unsupported debug directory")
            for i in range(debug_size//28):
                debug_off = rva_offset_existing(source, table, count,
                                                required_headers, debug_rva + 28*i, 28)
                adjust_file_pointer(debug_off + 24)
    wr32(opt+60, required_headers)  # SizeOfHeaders
    if section_head+40 > required_headers or section_head+40 > original_first_raw+header_delta:
        raise ValueError("expanded PE headers do not fit the additional section")
    if size_image < highest_va_end:
        raise ValueError("inconsistent existing SizeOfImage")
    # Bound import and Authenticode must not be silently invalidated.
    if rd32(opt+96+11*8) or rd32(opt+96+11*8+4):
        raise ValueError("bound imports present; no safe rewrite contract")
    if rd32(opt+96+4*8) or rd32(opt+96+4*8+4):
        raise ValueError("signed EXE: patch would invalidate signature")
    import_rva, import_size = rd32(opt+104), rd32(opt+108)
    if not import_rva or not 20 <= import_size <= 1024*1024:
        raise ValueError("no valid original import descriptors")

    def rva_offset(rva: int, size: int) -> int:
        if rva < size_headers and rva + size <= size_headers:
            return rva
        for i in range(count):
            row = table + i*40
            va, vsize, raw, raw_off = (
                rd32(row+12), rd32(row+8), rd32(row+16), rd32(row+20))
            if va <= rva and rva + size <= va + min(vsize or raw, raw):
                return raw_off + rva - va
        raise ValueError("unmapped original import table RVA")

    descriptors = []
    for i in range(min(4096, import_size // 20)):
        block = bytes(source[rva_offset(import_rva + i*20, 20):
                             rva_offset(import_rva + i*20, 20)+20])
        if block == bytes(20):
            break
        descriptors.append(block)
    else:
        raise ValueError("unterminated original import descriptor list")
    if len(descriptors) != len(original["imported_dlls"]):
        raise ValueError("original PE import descriptor count is inconsistent")

    new_va = highest_va_end
    desc_len = (len(descriptors) + 2) * 20
    name_off = align(desc_len, 4)
    ilt_off = align(name_off + len(NAME), 4)
    iat_off = align(ilt_off + 8, 4)
    data_size = iat_off + 8
    raw_size = align(data_size, file_align)
    new_raw = align(len(source), file_align)
    if new_va + align(data_size, section_align) > 0xffffffff:
        raise ValueError("PE size overflow")

    section = bytearray(raw_size)
    for i, descriptor in enumerate(descriptors):
        section[i*20:i*20+20] = descriptor
    # New import by ordinal 1 exported by the independent Wow335Loader.dll.
    struct.pack_into("<IIIII", section, len(descriptors)*20,
                     new_va + ilt_off, 0, 0, new_va + name_off, new_va + iat_off)
    section[name_off:name_off+len(NAME)] = NAME
    struct.pack_into("<II", section, ilt_off, 0x80000001, 0)
    struct.pack_into("<II", section, iat_off, 0x80000001, 0)

    section_header = struct.pack("<8sIIIIIIHHI", SECTION, data_size, new_va,
                                 raw_size, new_raw, 0, 0, 0, 0, 0xc0000040)
    source[section_head:section_head+40] = section_header
    wr16(pe + 6, count + 1)
    wr32(opt + 56, new_va + align(data_size, section_align))
    wr32(opt + 8, rd32(opt + 8) + raw_size)
    wr32(opt + 64, 0)  # Windows recalculates optional PE checksum.
    wr32(opt + 104, new_va)
    wr32(opt + 108, desc_len)
    source.extend(bytes(new_raw - len(source)))
    source.extend(section)

    patched = bytes(source)
    result = inspect_imports(patched)
    if result["imported_dlls"][:-1] != original["imported_dlls"] or result["imported_dlls"][-1] != {
        "dll": "Wow335Loader.dll", "functions": ["#1"]}:
        raise ValueError("import patch did not preserve original libraries and add exactly one import")

    return patched, {
        "kind": "IMPORT_APPEND_TEST_NOT_GAME_PACKAGE",
        "original_sha256": expected_sha256,
        "patched_sha256": hashlib.sha256(patched).hexdigest(),
        "patched_size": len(patched),
        "new_import": {"dll": "Wow335Loader.dll", "ordinal": 1},
        "original_imports_preserved": True,
        "original_epochconnection_preserved": any(
            x["dll"].casefold() == "epochconnection.dll" and "#1" in x["functions"]
            for x in result["imported_dlls"]),
        "patched_exe_requires_independent_loader_dll": True,
        "in_game_verified": False,
        "final_package": "NOT_RUN",
        "section": SECTION.decode("ascii"),
        "header_expansion_bytes": header_delta,
        "size_of_headers": required_headers,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default="Wow.exe")
    parser.add_argument("--target", default="runtime/client_exe_target.json")
    parser.add_argument("--out", default="dist/loader_preview/Wow335_Loader_TEST.exe")
    parser.add_argument("--report", default="dist/loader_preview/loader_patch_build.json")
    args = parser.parse_args()
    try:
        target = json.loads(Path(args.target).read_text(encoding="utf-8"))
        src = Path(args.exe)
        inspected = inspect_exe(src)
        verify_target(inspected, target)
        payload, report = patch(src.read_bytes(), target["sha256"])
        out = Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(payload)
        report_path = Path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n",
                               encoding="utf-8")
        print("LOADER_IMPORT_PREVIEW: PASS; original EpochConnection untouched;",
              report["patched_sha256"])
        print("NOT_GAME_PACKAGE: do not install or distribute as a complete playable client")
        return 0
    except (OSError, ValueError, TypeError, KeyError, struct.error) as exc:
        print("LOADER_IMPORT_PREVIEW: FAIL", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
