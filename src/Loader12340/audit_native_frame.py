"""Read-only audit of frame-update candidates in EXACT pinned 12340 Wow.exe.

This does NOT license installing a hook. Static RVA presence, a plausible vtable,
and an instruction boundary are necessary, not sufficient: the actual dispatch
thread, lifetime, and gameplay behavior still require verification.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

FRAME = {
    "CGWorldFrame::OnWorldUpdate": 0x004FA5F0,
    "CGWorldFrame::OnLayerUpdate": 0x004FA040,
    "CGWorldFrame::OnFrameRender": 0x004FB080,
}
WORLD_FRAME_GLOBAL = 0x00EEEA8C


def read_pe(path):
    data = Path(path).read_bytes()
    off = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[:2] == b"MZ" and data[off:off+4] == b"PE\\0\\0".replace(b"\\0", b"\x00")
    assert struct.unpack_from("<H", data, off+4)[0] == 0x14C
    opt = off+24
    assert struct.unpack_from("<H", data, opt)[0] == 0x10B
    base = struct.unpack_from("<I", data, opt+28)[0]
    count = struct.unpack_from("<H", data, off+6)[0]
    sh = opt+struct.unpack_from("<H", data, off+20)[0]
    sections = []
    for i in range(count):
        at = sh+i*40
        name = data[at:at+8].split(b"\x00")[0].decode("ascii", "replace")
        size, va, raw_size, raw = struct.unpack_from("<IIII", data, at+8)
        sections.append(dict(name=name, va=base+va, size=size,
                             raw=raw, raw_size=raw_size,
                             executable=bool(struct.unpack_from("<I", data, at+36)[0] & 0x20000000)))
    return data, base, sections


def va_file_offset(va, sections):
    for s in sections:
        if s["va"] <= va < s["va"] + min(s["size"], s["raw_size"]):
            return s["raw"] + va-s["va"], s
    return None, None


def run(exe, manifest):
    data, base, sections = read_pe(exe)
    target = json.loads(Path(manifest).read_text(encoding="utf-8"))
    digest = hashlib.sha256(data).hexdigest()
    if digest != target["sha256"] or len(data) != target["size"]:
        raise SystemExit("FRAME_AUDIT: FAIL wrong exact client")
    print(f"FRAME_AUDIT_CLIENT: PASS sha256={digest} size={len(data)} base=0x{base:08X}")
    for k, va in FRAME.items():
        off, section = va_file_offset(va, sections)
        if off is None or not section["executable"]:
            print(f"CANDIDATE_NOT_EXECUTABLE {k} 0x{va:08X}")
            continue
        print(f"FRAME_CANDIDATE {k} 0x{va:08X} section={section['name']} bytes={data[off:off+32].hex()}")
        pattern = struct.pack("<I", va)
        total = 0
        for s in sections:
            if s["executable"] or not s["raw_size"]: continue
            chunk = data[s["raw"]:s["raw"]+s["raw_size"]]
            found = []
            start = 0
            while True:
                at = chunk.find(pattern, start)
                if at < 0: break
                if at % 4 == 0:
                    vaddr = s["va"]+at
                    found.append(vaddr)
                start = at+1
            for address in found[:40]:
                o, _ = va_file_offset(address, sections)
                before = data[o-32:o] if o is not None and o >= 32 else b""
                after = data[o+4:o+36] if o is not None else b""
                print(f"  DATA_POINTER va=0x{address:08X} before={before.hex()} after={after.hex()}")
            total += len(found)
        print(f"  DATA_POINTER_TOTAL {k} {total}")
    off, section = va_file_offset(WORLD_FRAME_GLOBAL, sections)
    print(f"WORLD_FRAME_GLOBAL 0x{WORLD_FRAME_GLOBAL:08X} section={section['name'] if section else 'NOT_MAPPED'}"
          f" raw={data[off:off+4].hex() if off is not None else 'unavailable'}")
    print("FRAME_AUDIT: CANDIDATES_ONLY; no hook deployed and no game-thread proof.")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--exe", default="Wow.exe")
    p.add_argument("--manifest", default="runtime/client_exe_target.json")
    args = p.parse_args()
    run(args.exe, args.manifest)
