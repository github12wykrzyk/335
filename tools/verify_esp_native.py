"""Verify actual ESP native Windows PE32 x86 DLL, not a game-package gate."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def inspect(path: Path, symbols: str):
    data = path.read_bytes()
    if len(data) < 512 or data[:2] != b"MZ":
        raise ValueError("not PE")
    off = struct.unpack_from("<I", data, 0x3c)[0]
    if off + 26 > len(data) or data[off:off+4] != b"PE\x00\x00":
        raise ValueError("invalid PE signature")
    machine, flags, magic = (struct.unpack_from("<H", data, off+i)[0]
                             for i in (4, 22, 24))
    if machine != 0x14c or magic != 0x10b or not flags & 0x2000:
        raise ValueError("not a Windows PE32 x86 DLL")
    required = ("W335_MessageId", "W335_HookProc", "W335_CallWndProc")
    for symbol in required:
        if symbol not in symbols:
            raise ValueError("missing canonical loader export: "+symbol)
    return {"file": path.name, "sha256": hashlib.sha256(data).hexdigest(),
            "size": len(data), "machine": "PE32 I386 DLL",
            "exports": required, "gameplay_verified": False,
            "active_runtime": False, "package_ready": False}

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--dll", type=Path, required=True)
    p.add_argument("--exports", type=Path, required=True)
    p.add_argument("--report", type=Path, required=True)
    args = p.parse_args()
    result = inspect(args.dll, args.exports.read_text(encoding="utf-8", errors="replace"))
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print("ESP_NATIVE_PE32_X86: PASS; NOT_GAME_PACKAGE; sha256="+result["sha256"])

if __name__ == "__main__":
    main()
