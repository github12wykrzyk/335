"""Read-only import audit for the exact pinned WoW 3.3.5a / build 12340 PE32 x86 client.

No code execution, patching, DLL loading or package readiness assertion.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from verify_client_exe import inspect_exe, verify_target


def inspect_imports(data: bytes) -> dict:
    def u16(off):
        if off < 0 or off + 2 > len(data):
            raise ValueError("truncated PE 16-bit field")
        return struct.unpack_from("<H", data, off)[0]

    def u32(off):
        if off < 0 or off + 4 > len(data):
            raise ValueError("truncated PE 32-bit field")
        return struct.unpack_from("<I", data, off)[0]

    if len(data) < 512 or data[:2] != b"MZ":
        raise ValueError("missing DOS header")
    pe = u32(0x3c)
    if pe < 64 or pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("invalid PE header")
    if u16(pe + 4) != 0x14c or u16(pe + 24) != 0x10b:
        raise ValueError("expected PE32 i386")
    count, opt_size, opt = u16(pe + 6), u16(pe + 20), pe + 24
    table = opt + opt_size
    if opt_size < 224 or not 0 < count <= 96 or table + count * 40 > len(data):
        raise ValueError("invalid PE32 section/data-directory bounds")
    image_base, header_size, directories = u32(opt + 28), u32(opt + 60), u32(opt + 92)
    if directories < 2:
        raise ValueError("missing import data-directory")
    sections = []
    for index in range(count):
        off = table + index * 40
        vsize, va, raw_size, raw_off = (
            u32(off + 8), u32(off + 12), u32(off + 16), u32(off + 20))
        if raw_size and raw_off + raw_size > len(data):
            raise ValueError("section extends past file")
        sections.append((va, vsize, raw_size, raw_off))

    def rva_offset(rva, length=1):
        if rva < header_size and rva + length <= header_size and rva + length <= len(data):
            return rva
        for va, vsize, raw_size, raw_off in sections:
            if va <= rva and rva + length <= va + min(vsize or raw_size, raw_size):
                off = raw_off + rva - va
                if off + length <= len(data):
                    return off
        raise ValueError("unmapped import RVA: 0x%x" % rva)

    def name_at(rva):
        off = rva_offset(rva)
        end = data.find(b"\0", off, min(len(data), off + 512))
        if end < 0:
            raise ValueError("unterminated import string")
        return data[off:end].decode("ascii", "strict")

    import_rva, import_size = u32(opt + 104), u32(opt + 108)
    if bool(import_rva) != bool(import_size):
        raise ValueError("inconsistent import directory")
    imported = []
    if import_rva:
        if not 20 <= import_size <= 1024 * 1024:
            raise ValueError("invalid import directory size")
        terminated = False
        for index in range(min(4096, import_size // 20)):
            off = rva_offset(import_rva + index * 20, 20)
            oft, stamp, forward, name_rva, first = struct.unpack_from("<IIIII", data, off)
            if not any((oft, stamp, forward, name_rva, first)):
                terminated = True
                break
            if not name_rva or not (oft or first):
                raise ValueError("invalid import descriptor")
            functions = []
            thunk = oft or first
            for j in range(65536):
                value = u32(rva_offset(thunk + 4 * j, 4))
                if value == 0:
                    break
                functions.append("#" + str(value & 0xffff)
                                 if value & 0x80000000 else name_at(value + 2))
            else:
                raise ValueError("unterminated import thunk table")
            imported.append({"dll": name_at(name_rva), "functions": functions})
        if not terminated:
            raise ValueError("unterminated import descriptor table")
    markers = ("twloader.dll", "dlls.txt", "func1")
    return {
        "image_base": "0x%08x" % image_base,
        "import_directory_rva": "0x%08x" % import_rva,
        "import_directory_size": import_size,
        "imported_dlls": imported,
        "legacy_loader_imported": any(
            entry["dll"].casefold() == "twloader.dll" for entry in imported),
        "legacy_loader_markers_ascii": {
            m: m.encode("ascii") in data for m in markers},
        "legacy_loader_markers_utf16": {
            m: m.encode("utf-16le") in data for m in markers},
        "scope": "static import directory and literal markers; not runtime/patch/loader proof",
    }


def audit(path: Path, target: dict) -> dict:
    client = inspect_exe(path)
    verify_target(client, target)
    return {
        "schema_version": 1,
        "project": "335",
        "target_build": 12340,
        "client": client,
        "imports": inspect_imports(path.read_bytes()),
        "conclusion": "STATIC_AUDIT_ONLY_NOT_LOADER_NOT_GAME_PACKAGE",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default="Wow.exe")
    ap.add_argument("--target", default="runtime/client_exe_target.json")
    ap.add_argument("--report", default="dist/loader_12340_audit.json")
    args = ap.parse_args()
    try:
        result = audit(Path(args.exe),
                       json.loads(Path(args.target).read_text(encoding="utf-8")))
        out = Path(args.report)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                       encoding="utf-8")
        print("LOADER_12340_AUDIT: PASS; exact client; imported libraries:",
              len(result["imports"]["imported_dlls"]))
        print("LOADER_12340_READY: NO; no loader or game package was built")
        return 0
    except (OSError, ValueError, UnicodeError, struct.error, KeyError, TypeError) as exc:
        print("LOADER_12340_AUDIT: FAIL", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
