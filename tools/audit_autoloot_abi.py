"""Read-only PE32 x86 ABI reconnaissance for the exact pinned WoW 12340 EXE.

This script DOES NOT establish callable signatures or in-game compatibility.
All named addresses are external research leads; a successful report is not
an active-DLL, hook or package verification.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Potential symbols reported for *standard* 3.3.5a 12340; treat each as a
# candidate until independently analyzed against our exact selected EXE.
FUNCTION_CANDIDATES = {
    "ObjectPointer": 0x004D4DB0,
    "UnitRightClick": 0x00731260,
    "ObjectPosition": 0x004D5EA0,
    "UnitPosition": 0x006E6F10,
    "FrameScriptExecute": 0x00819210,
    "LootSlot": 0x00589140,
    "LootResponse": 0x006D53B0,
    "LootReleaseResponse": 0x006D59E0,
}
DATA_CANDIDATES = {
    "ClientConnectionPointer": 0x00C79CE0,
    "LootWindow": 0x00BFA8D8,
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=ROOT / "Wow.exe")
    ap.add_argument("--out", type=Path, default=ROOT / "dist/autoloot_abi.json")
    args = ap.parse_args()
    target = json.loads((ROOT / "runtime/client_exe_target.json").read_text(encoding="utf-8"))
    data = args.exe.read_bytes()
    sha256 = hashlib.sha256(data).hexdigest()
    if sha256 != target["sha256"] or len(data) != target["size"]:
        raise ValueError("exact user-selected Wow.exe SHA256/size mismatch")
    if data[:2] != b"MZ":
        raise ValueError("missing MZ")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("missing PE signature")
    machine, count = struct.unpack_from("<HH", data, pe + 4)
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if machine != 0x14C or struct.unpack_from("<H", data, opt)[0] != 0x10B:
        raise ValueError("not PE32 x86")
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    sections = []
    for i in range(count):
        offset = opt + optional_size + i * 40
        name, virtual_size, virtual_address, raw_size, raw_offset, _, _, _, _, flags = (
            struct.unpack_from("<8sIIIIIIHHI", data, offset)
        )
        sections.append({
            "name": name.split(b"\0", 1)[0].decode("ascii", "replace"),
            "virtual_address": virtual_address,
            "virtual_size": virtual_size,
            "raw_offset": raw_offset,
            "raw_size": raw_size,
            "flags": flags,
        })

    def read_va(va: int, n: int, executable: bool) -> tuple[bytes, str]:
        rva = va - image_base
        for section in sections:
            if (section["virtual_address"] <= rva <
                    section["virtual_address"] + (section["raw_size"] if executable else max(section["raw_size"], section["virtual_size"]))):
                if executable and not section["flags"] & 0x20000000:
                    raise ValueError(f"{va:#x} is not in an executable section")
                if not executable and not section["flags"] & 0x40000000:
                    raise ValueError(f"{va:#x} is not readable")
                off = section["raw_offset"] + rva - section["virtual_address"]
                end = min(off + n, section["raw_offset"] + section["raw_size"])
                if off < 0 or end > len(data) or end - off < n:
                    if not executable:
                        return b"", section["name"]  # uninitialized global / BSS
                    raise ValueError(f"truncated candidate {va:#x}")
                return data[off:end], section["name"]
        raise ValueError(f"candidate outside PE raw sections: {va:#x}")

    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    except ImportError as exc:
        raise RuntimeError("capstone 5.x required to audit actual instructions") from exc

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    functions = {}
    for name, va in FUNCTION_CANDIDATES.items():
        raw, section = read_va(va, 96, executable=True)
        instructions = [
            {"address": f"0x{ins.address:08X}", "mnemonic": ins.mnemonic, "operands": ins.op_str}
            for ins in md.disasm(raw, va) if ins.address < va + 64
        ]
        if len(instructions) < 4 or raw[:8] in (b"\x00" * 8, b"\xCC" * 8):
            raise ValueError(f"candidate {name} missing plausible instructions")
        functions[name] = {
            "va": f"0x{va:08X}", "section": section, "bytes_hex": raw[:64].hex(),
            "disassembly": instructions,
            "status": "static_candidate_only_not_calling_convention_verified",
        }
    globals_ = {}
    for name, va in DATA_CANDIDATES.items():
        raw, section = read_va(va, 8, executable=False)
        globals_[name] = {
            "va": f"0x{va:08X}", "section": section, "bytes_hex": raw.hex(),
            "status": "static_candidate_only_runtime_value_unknown",
        }
    report = {
        "client_sha256": sha256, "size": len(data),
        "image_base": f"0x{image_base:08X}", "target_build": target["target_build"],
        "functions": functions, "globals": globals_,
        "conclusion": "STATIC_CANDIDATES_ONLY; native ABI/thread/loot ownership NOT verified",
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("AUTOLOOT_STATIC_ABI: PASS read-only exact-client address and disassembly audit")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
