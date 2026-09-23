"""Audit the supplied WoW 3.3.5a reference EXE without executing it."""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path

def inspect_exe(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 512 or data[:2] != b"MZ":
        raise ValueError("missing/truncated MZ executable")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if pe < 0x40 or pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("invalid PE signature or header bounds")
    machine, sections, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", data, pe + 4)
    if machine != 0x014C:
        raise ValueError(f"not x86 (COFF machine 0x{machine:04x})")
    opt = pe + 24
    if optional_size < 96 or opt + optional_size > len(data):
        raise ValueError("truncated PE32 optional header")
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10B:
        raise ValueError(f"not PE32 (magic 0x{magic:04x})")
    entry = struct.unpack_from("<I", data, opt + 16)[0]
    image_size = struct.unpack_from("<I", data, opt + 56)[0]
    subsystem = struct.unpack_from("<H", data, opt + 68)[0]
    if not sections or not entry or not image_size or subsystem not in (2, 3):
        raise ValueError("not a valid executable image (sections, entrypoint, image size or subsystem)")
    section_table_end = opt + optional_size + 40 * sections
    if section_table_end > len(data):
        raise ValueError("truncated PE section table")
    return {
        "name": path.name,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "pe_machine": "0x014c",
        "pe_format": "PE32",
        "entrypoint_rva": entry,
        "image_size": image_size,
        "subsystem": subsystem,
        "sections": sections,
        "client_build": "not_proven_by_pe_header",
        "verification": "PE32_X86_PASS",
    }

def verify_target(result: dict, target: dict) -> None:
    """Reject other EXEs even if they report the same game build."""
    for key in ("name", "sha256", "size"):
        if result.get(key) != target.get(key):
            raise ValueError("selected client mismatch: " + key)
    if target.get("target_build") != 12340 or target.get("architecture") != "x86":
        raise ValueError("invalid selected client target build / architecture")
    if result.get("pe_machine") != "0x014c" or result.get("pe_format") != "PE32":
        raise ValueError("selected client must be PE32 x86")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default="Wow.exe")
    parser.add_argument("--report", default="dist/client_exe_audit.json")
    parser.add_argument("--require-target", action="store_true")
    args = parser.parse_args()
    try:
        result = inspect_exe(Path(args.exe))
        if args.require_target:
            target = json.loads((Path(__file__).resolve().parents[1] / "runtime/client_exe_target.json").read_text(encoding="utf-8"))
            verify_target(result, target)
            result["exact_client_target"] = "PASS"
        dest = Path(args.report)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print("CLIENT_EXE_AUDIT: PASS", result["name"], result["sha256"], result["size"])
        print("CLIENT_BUILD: unconfirmed by PE header; check Win32 file-version resource separately")
        return 0
    except (OSError, ValueError, struct.error) as exc:
        print("CLIENT_EXE_AUDIT: FAIL", exc)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
