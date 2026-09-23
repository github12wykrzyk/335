"""Verify exact-byte recoverability of every STABLE DLL before promotion."""
from __future__ import annotations
import hashlib
import json
import lzma
import re
import sys
from pathlib import Path
from manifest_common import ROOT, load_json, repo_path, sha256_file

def verify_exact_artifacts(runtime):
    errors = []
    files = runtime.get("files", [])
    exes = [x for x in files if x.get("kind") == "exe"]
    dlls = [x for x in files if x.get("kind") == "dll"]
    if runtime.get("state") == "empty" or len(exes) != 1 or not dlls:
        return ["stable requires exactly one verified EXE and at least one DLL"]
    for item in files:
        try:
            path = repo_path(item["path"])
            if not path.is_file() or sha256_file(path) != item["sha256"]:
                errors.append(item["component"] + ": registered runtime bytes missing/mismatched")
                continue
            if item.get("kind") != "dll":
                continue
            artifact = item.get("binary_artifact")
            if not isinstance(artifact, dict) or artifact.get("kind") != "xz":
                errors.append(item["component"] + ": exact xz binary_artifact required")
                continue
            cache_path = artifact.get("path")
            if not isinstance(cache_path, str) or not re.fullmatch(
                    r"artifacts/runtime_cache/[0-9a-f]{64}\.dll\.xz", cache_path):
                errors.append(item["component"] + ": invalid content-addressed cache path")
                continue
            if artifact.get("sha256") != item["sha256"] or item["sha256"] not in cache_path:
                errors.append(item["component"] + ": cache identity does not match manifest")
                continue
            compressed = repo_path(cache_path)
            if not compressed.is_file():
                errors.append(item["component"] + ": exact DLL cache missing")
                continue
            if not isinstance(artifact.get("size"), int) or not 0 < artifact["size"] < 128 * 1024 * 1024:
                errors.append(item["component"] + ": invalid cache uncompressed size")
                continue
            # Bounded decompression: never trust unbounded archived artifact bytes.
            with lzma.open(compressed, "rb") as data:
                decompressed = data.read(artifact["size"] + 1)
                if len(decompressed) != artifact["size"] or data.read(1):
                    errors.append(item["component"] + ": cache decompression size mismatch")
                elif hashlib.sha256(decompressed).hexdigest() != item["sha256"]:
                    errors.append(item["component"] + ": exact cache SHA256 mismatch")
        except (OSError, ValueError, KeyError, TypeError, lzma.LZMAError) as exc:
            errors.append(str(item.get("component", "unknown")) + ": " + str(exc))
    return errors

def main():
    try:
        current = load_json(ROOT / "CURRENT.json")
        runtime = load_json(ROOT / "runtime/current.json")
        errors = verify_exact_artifacts(runtime)
        if current.get("status") != "stable" or not current.get("stable_baseline"):
            errors.append("stable status and accepted baseline required")
        if errors:
            for error in errors:
                print("EXACT_RUNTIME: FAIL", error)
            return 1
        print("EXACT_RUNTIME: PASS; exact DLL bytes recoverable")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print("EXACT_RUNTIME: FAIL", exc)
        return 1

if __name__ == "__main__":
    sys.exit(main())
