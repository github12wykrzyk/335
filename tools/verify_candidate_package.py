"""Fail-closed package and exact GitHub source-ref verification."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import zipfile
from pathlib import Path
from manifest_common import ROOT, repo_path, sha256_file

def pe32(data, kind):
    if len(data) < 512 or data[:2] != b"MZ":
        return False
    off = struct.unpack_from("<I", data, 0x3c)[0]
    if off < 0x40 or off + 26 > len(data) or data[off:off+4] != b"PE\0\0":
        return False
    machine = struct.unpack_from("<H", data, off+4)[0]
    magic = struct.unpack_from("<H", data, off+24)[0]
    flags = struct.unpack_from("<H", data, off+22)[0]
    return (machine == 0x14c and magic == 0x10b and
            bool(flags & 0x2000) == (kind == "dll"))

def verify_attestation(meta, sha, branch, manifest_sha, registry_sha):
    if meta.get("schema_version") != 2 or meta.get("project") != "335":
        raise ValueError("invalid candidate metadata schema")
    if meta.get("wow_build") != 12340 or meta.get("arch") != "x86":
        raise ValueError("wrong game build or architecture")
    if sha and (meta.get("git_sha") != sha or meta.get("branch") != branch):
        raise ValueError("package provenance does not match exact checkout SHA/ref")
    if meta.get("manifest_sha256") != manifest_sha:
        raise ValueError("runtime manifest changed since packaging")
    if meta.get("module_registry_sha256") != registry_sha:
        raise ValueError("module registry changed since packaging")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--package", required=True)
    ap.add_argument("--metadata", required=True)
    ap.add_argument("--finalize", action="store_true")
    args = ap.parse_args()
    try:
        package = repo_path(args.package)
        meta = json.loads(repo_path(args.metadata).read_text(encoding="utf-8"))
        sha = os.getenv("GITHUB_SHA")
        branch = os.getenv("GITHUB_REF_NAME")
        if os.getenv("GITHUB_ACTIONS") == "true":
            if not sha or not branch or not re.fullmatch(r"[0-9a-f]{40}", sha):
                raise ValueError("no trustworthy GitHub SHA/ref in CI")
            actual = subprocess.check_output(["git", "rev-parse", "HEAD"],
                cwd=ROOT, text=True).strip()
            if actual != sha:
                raise ValueError("CI checkout does not match GITHUB_SHA")
        verify_attestation(meta, sha, branch,
            sha256_file(ROOT / "runtime/current.json"),
            sha256_file(ROOT / "runtime/module_registry.json"))
        runtime = json.loads((ROOT / "runtime/current.json").read_text(encoding="utf-8"))
        registry = json.loads((ROOT / "runtime/module_registry.json").read_text(encoding="utf-8"))
        expected = runtime["files"]
        if [x.get("component") for x in meta["files"]] != [
                x.get("component") for x in expected]:
            raise ValueError("package component set/order differs from active manifest")
        if [x.get("sha256") for x in meta["files"]] != [
                x.get("sha256") for x in expected]:
            raise ValueError("package file identity differs from active manifest")
        fingerprints = {m["component"]: {
            src: sha256_file(repo_path(src)) for src in m["sources"]}
            for m in registry["modules"]}
        if meta.get("source_fingerprints") != fingerprints:
            raise ValueError("registered source fingerprint drift")
        if meta.get("module_contracts") != [{
                "component": m["component"], "requires": m["requires"],
                "resources": m["resources"]} for m in registry["modules"]]:
            raise ValueError("package module contracts differ from registry")
        if not re.fullmatch(r"[0-9a-f]{64}", meta["package_sha256"]):
            raise ValueError("invalid package SHA")
        if (meta["package_name"] != package.name or
                meta["package_size"] != package.stat().st_size or
                meta["package_sha256"] != sha256_file(package)):
            raise ValueError("package SHA256/size mismatch")
        with zipfile.ZipFile(package) as archive:
            names = archive.namelist()
            if len(names) != len(set(n.casefold() for n in names)):
                raise ValueError("duplicate ZIP name")
            if (not all("/" not in n and "\\" not in n and n not in (".", "..")
                        for n in names) or
                    names != [x["name"] for x in meta["files"]] + ["dlls.txt"]):
                raise ValueError("incorrect ZIP root file set or order")
            exes = [x for x in meta["files"] if x["kind"] == "exe"]
            dlls = [x for x in meta["files"] if x["kind"] == "dll"]
            selected = json.loads((ROOT / "runtime/client_exe_target.json").read_text(encoding="utf-8"))
            if (len(exes) != 1 or not dlls or
                    meta["exe"] != exes[0]["name"] or
                    exes[0]["name"] != selected["name"] or
                    exes[0]["sha256"] != selected["sha256"] or
                    meta["dlls"] != [x["name"] for x in dlls]):
                raise ValueError("missing/extra EXE or DLL or incorrect DLL order")
            if archive.read("dlls.txt") != (
                    "\r\n".join(meta["dlls"]) + "\r\n").encode("ascii"):
                raise ValueError("dlls.txt differs from active DLL load order")
            for x in meta["files"]:
                data = archive.read(x["name"])
                if (not re.fullmatch(r"[0-9a-f]{64}", x["sha256"]) or
                        hashlib.sha256(data).hexdigest() != x["sha256"] or
                        not pe32(data, x["kind"])):
                    raise ValueError("DLL/EXE hash, PE32 x86 or PE kind mismatch: " + x["name"])
        print("FINAL_PACKAGE: PASS; build 12340 x86; branch:", meta["branch"],
              "git_sha:", meta["git_sha"])
        return 0
    except (OSError, ValueError, KeyError, TypeError, AssertionError,
            struct.error, zipfile.BadZipFile) as exc:
        print("FINAL_PACKAGE: FAIL", exc)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
