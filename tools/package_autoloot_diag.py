"""Build a deterministic, clearly NON-INSTALLABLE diagnostic WoW addon ZIP.

Never use this artifact as a game package, updater candidate or native DLL.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/AutoLoot/diagnostics"
FILES = (
    "WoW335AutoLootDiag/WoW335AutoLootDiag.lua",
    "WoW335AutoLootDiag/WoW335AutoLootDiag.toc",
    "README_TEST.txt",
)
CLIENT_SHA = "2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d"


def build(output: Path, git_sha: str, branch: str) -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", git_sha):
        raise ValueError("diagnostic ZIP needs exact repository commit SHA")
    if branch != "feature/autoloot-12340":
        raise ValueError("diagnostic belongs only to the isolated AutoLoot experiment")
    data = {name: (SOURCE / name).read_bytes() for name in FILES}
    if not data[FILES[1]].splitlines() or data[FILES[1]].splitlines()[0].strip() != b"## Interface: 30300":
        raise ValueError("incorrect 3.3.5 addon interface version")
    meta = {
        "kind": "MANUAL_LOOT_WINDOW_DIAGNOSTIC_ADDON_NOT_GAME_PACKAGE",
        "project": "335",
        "branch": branch,
        "git_sha": git_sha,
        "target_build": 12340,
        "client_exe_sha256": CLIENT_SHA,
        "auto_corpse_interaction": False,
        "native_dll": False,
        "final_package": "NOT_RUN",
        "files_sha256": {p: hashlib.sha256(data[p]).hexdigest() for p in FILES},
    }
    data["diag_info.json"] = (json.dumps(meta, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, content in data.items():
            zi = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            zi.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(zi, content)
    print("AUTOLOOT_DIAGNOSTIC_ADDON: PASS; NOT A GAME PACKAGE")
    print("ADDON_ZIP_SHA256:", hashlib.sha256(output.read_bytes()).hexdigest())
    print("EXACT_GIT_SHA:", git_sha)
    return meta


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / "dist/WoW335_AutoLoot_DIAG_Addon_12340.zip")
    parser.add_argument("--sha", default=os.environ.get("GITHUB_SHA", ""))
    parser.add_argument("--branch", default=os.environ.get("GITHUB_REF_NAME", "feature/autoloot-12340"))
    args = parser.parse_args()
    build(args.output, args.sha, args.branch)
