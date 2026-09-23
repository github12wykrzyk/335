"""Package only the exact registered client and active DLL set for WoW 12340."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import zipfile
from pathlib import Path
from manifest_common import ROOT, load_json, repo_path, sha256_file

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--package", required=True)
    ap.add_argument("--metadata", required=True)
    ap.add_argument("--native-build-report")
    args = ap.parse_args()
    for verifier in ("verify_repo.py", "verify_current.py"):
        if subprocess.call([sys.executable, str(ROOT / "tools" / verifier)], cwd=ROOT):
            return 1
    runtime = load_json(ROOT / "runtime/current.json")
    registry = load_json(ROOT / "runtime/module_registry.json")
    files = runtime["files"]
    exes = [x for x in files if x.get("kind") == "exe"]
    dlls = [x for x in files if x.get("kind") == "dll"]
    selected = load_json(ROOT / "runtime/client_exe_target.json")
    if exes and (exes[0].get("path") != selected["path"] or
                 exes[0].get("sha256") != selected["sha256"] or
                 sha256_file(repo_path(exes[0]["path"])) != selected["sha256"]):
        print("PACKAGE: FAIL selected client EXE mismatch")
        return 3
    if runtime.get("state") == "empty" or len(exes) != 1 or not dlls:
        print("NO_RUNTIME: exact 12340 EXE and one or more active real DLLs required")
        return 2
    if any(x.get("kind") not in ("exe", "dll") for x in files):
        print("PACKAGE: FAIL unsupported runtime data entry")
        return 3
    branch = os.getenv("GITHUB_REF_NAME") or "local"
    commit = os.getenv("GITHUB_SHA") or "local-uncommitted"
    if os.getenv("GITHUB_ACTIONS") == "true":
        if not re.fullmatch(r"[0-9a-f]{40}", commit) or branch == "local":
            print("PACKAGE: FAIL no GitHub ref/SHA attestation")
            return 3
        if subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                   text=True).strip() != commit:
            print("PACKAGE: FAIL GITHUB_SHA differs from checked-out commit")
            return 3
    if args.native_build_report:
        report = load_json(repo_path(args.native_build_report))
        if report.get("git_sha") != commit or report.get("build") != 12340:
            print("PACKAGE: FAIL native build provenance is not from this SHA")
            return 3
        rebuilt = report.get("results", [])
        if set(report.get("selected_components", [])) != {
                row.get("component") for row in rebuilt}:
            print("PACKAGE: FAIL incomplete native rebuild coverage")
            return 3
        if any(row.get("verification") != "EXACT_PE32_X86_REBUILD_PASS" for row in rebuilt):
            print("PACKAGE: FAIL native rebuild not attested")
            return 3
    elif os.getenv("GITHUB_ACTIONS") == "true" and branch not in ("main",):
        print("PACKAGE: FAIL TEST package needs exact-SHA native build report")
        return 3
    names = [Path(x["path"]).name for x in files]
    if len({n.casefold() for n in names}) != len(names) or any(
            n in ("dlls.txt", "candidate_metadata.json") for n in names):
        print("PACKAGE: FAIL duplicate/forbidden file name")
        return 3
    order = [x["component"] for x in files]
    for x in files:
        if any(dep not in order or order.index(dep) >= order.index(x["component"])
               for dep in x.get("depends_on", [])):
            print("PACKAGE: FAIL dependency load order", x["component"])
            return 3
    output = repo_path(args.package)
    output.parent.mkdir(parents=True, exist_ok=True)
    dll_order = [Path(x["path"]).name for x in dlls]
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for x in files:
            path = repo_path(x["path"])
            zi = zipfile.ZipInfo(path.name, (1980, 1, 1, 0, 0, 0))
            zi.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(zi, path.read_bytes())
        zi = zipfile.ZipInfo("dlls.txt", (1980, 1, 1, 0, 0, 0))
        archive.writestr(zi, ("\r\n".join(dll_order) + "\r\n").encode("ascii"))
    source_fingerprints = {m["component"]: {
        source: sha256_file(repo_path(source)) for source in m["sources"]}
        for m in registry["modules"]}
    metadata = {
        "schema_version": 2, "project": "335", "wow_build": 12340,
        "arch": "x86", "branch": branch, "git_sha": commit,
        "manifest_sha256": sha256_file(ROOT / "runtime/current.json"),
        "module_registry_sha256": sha256_file(ROOT / "runtime/module_registry.json"),
        "source_fingerprints": source_fingerprints,
        "module_contracts": [{"component": m["component"],
            "requires": m["requires"], "resources": m["resources"]}
            for m in registry["modules"]],
        "package_name": output.name, "package_sha256": sha256_file(output),
        "package_size": output.stat().st_size,
        "exe": Path(exes[0]["path"]).name, "dlls": dll_order,
        "files": [{"name": Path(x["path"]).name, "sha256": x["sha256"],
                   "component": x["component"], "kind": x["kind"]}
                  for x in files],
    }
    dest = repo_path(args.metadata)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print("CANDIDATE PACKAGE:", output, "sha256:", metadata["package_sha256"])
    return 0

if __name__ == "__main__":
    sys.exit(main())
