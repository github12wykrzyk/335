"""Read-only inventory of the two uploaded ConsoleXP 1.0.1 ZIP archives.

No archive member is extracted or executed; no reference binary enters a
runtime manifest. Git blob identities pin the actual files uploaded to work.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
REFERENCES = (
    ("source", "reference/ConsoleXP/ConsoleXP-1.0.1 source.zip",
     "b64e6e2c5c22a55d0712bf314aeb2c05b0ac959e"),
    ("release", "reference/ConsoleXP/ConsoleXP-1.0.1.zip",
     "a8424592a28fa43d8b8e0061d49e55ad8055630c"),
)
MAX_ARCHIVE_BYTES = 8 * 1024 * 1024
MAX_MEMBERS = 4096
MAX_MEMBER_BYTES = 8 * 1024 * 1024
MAX_TOTAL_BYTES = 48 * 1024 * 1024


def git_blob_sha1(raw: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()


def safe_member(name: str) -> bool:
    if not name or "\\" in name or "\0" in name or re.match(r"^[a-zA-Z]:", name):
        return False
    path = PurePosixPath(name)
    return not path.is_absolute() and all(part not in (".", "..") for part in name.split("/"))


def inspect_archive(archive: Path, expected_blob: str, role: str) -> dict:
    raw = archive.read_bytes()
    if len(raw) > MAX_ARCHIVE_BYTES:
        raise ValueError(f"{role}: archive exceeds maximum size")
    blob = git_blob_sha1(raw)
    if blob != expected_blob:
        raise ValueError(f"{role}: uploaded ZIP blob changed: {blob}; expected {expected_blob}")
    source_count = 0
    binary_names = []
    license_names = []
    headers = []
    total = 0
    seen = set()
    with zipfile.ZipFile(archive) as z:
        members = z.infolist()
        if len(members) > MAX_MEMBERS:
            raise ValueError(f"{role}: too many ZIP members")
        for member in members:
            name = member.filename
            if not safe_member(name):
                raise ValueError(f"{role}: unsafe ZIP member: {name!r}")
            key = name.rstrip("/").casefold()
            if key in seen:
                raise ValueError(f"{role}: duplicate ZIP member: {name!r}")
            seen.add(key)
            if member.is_dir():
                continue
            # Unix symlink, Windows reparse points, encrypted entries and high
            # expansion ratios cannot be approved as inert reference files.
            if (member.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError(f"{role}: symlink in ZIP: {name!r}")
            if member.flag_bits & 1:
                raise ValueError(f"{role}: encrypted ZIP member: {name!r}")
            if member.file_size > MAX_MEMBER_BYTES:
                raise ValueError(f"{role}: oversized ZIP member: {name!r}")
            total += member.file_size
            if total > MAX_TOTAL_BYTES:
                raise ValueError(f"{role}: total uncompressed size exceeds limit")
            if member.file_size and member.file_size > 250 * max(member.compress_size, 1):
                raise ValueError(f"{role}: excessive compression ratio: {name!r}")
            # Reading every member checks CRC, without writing or executing it.
            with z.open(member) as stream:
                while stream.read(65536):
                    pass
            suffix = PurePosixPath(name).suffix.lower()
            basename = PurePosixPath(name).name.lower()
            if suffix in {".c", ".cc", ".cpp", ".h", ".hpp"}:
                source_count += 1
                if any(token in name.lower() for token in ("camera", "game", "target", "hook")):
                    headers.append(name)
            if suffix in {".dll", ".exe", ".lib", ".sys"}:
                binary_names.append(name)
            if basename.startswith(("license", "licence", "copying")):
                license_names.append(name)
    if role == "source" and source_count == 0:
        raise ValueError("source: no native source files found")
    return {
        "role": role, "path": archive.relative_to(ROOT).as_posix(),
        "git_blob_sha1": blob, "sha256": hashlib.sha256(raw).hexdigest(),
        "archive_bytes": len(raw), "members": len(members),
        "uncompressed_bytes": total, "native_source_files": source_count,
        "relevant_source_paths": sorted(headers)[:40],
        "license_paths": sorted(license_names)[:20],
        "native_binary_paths": sorted(binary_names)[:40],
        "license_review_required": role == "source" and not license_names,
        "runtime_eligible": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", default="dist/consolexp_reference_audit.json")
    args = parser.parse_args()
    results = [inspect_archive(ROOT / path, sha, role)
               for role, path, sha in REFERENCES]
    output = ROOT / args.report
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({
        "purpose": "inert reference inventory; NOT a client or DLL compatibility test",
        "exact_client_abi_verified": False,
        "reference_archives": results,
    }, indent=2) + "\n", encoding="utf-8")
    print("CONSOLEXP_REFERENCE: PASS; archive identity, paths and CRC verified; no binaries executed")
    for item in results:
        print(item["role"], "members", item["members"], "native sources", item["native_source_files"],
              "license review required", item["license_review_required"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
