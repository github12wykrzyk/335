"""Build registered 12340 Windows x86 DLLs without a per-module workflow step.

Each DLL has explicit source/build recipe in runtime/module_registry.json.
The compiled byte identity MUST match the registered runtime binary. This strict
gate prevents CI from packaging a stale DLL after sources are edited.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from manifest_common import ROOT, load_json, repo_path, sha256_file
from verify_module_registry import validate
from native_toolchain import VCVARS_ARGS, PINNED_VC_VERSION, PINNED_WINDOWS_SDK

def select_modules(registry, base):
    modules = registry["modules"]
    if base is None:
        return {m["component"] for m in modules}
    if not re.fullmatch(r"[0-9a-fA-F]{40}", base):
        raise ValueError("--base must be a 40-character commit SHA")
    changed = set(subprocess.check_output(
        ["git", "diff", "--name-only", base, "HEAD", "--"], cwd=ROOT,
        text=True).splitlines())
    if changed & {"runtime/current.json", "runtime/module_registry.json",
                  "tools/build_active.py"}:
        return {m["component"] for m in modules}
    selected = {m["component"] for m in modules if changed.intersection(m["sources"])}
    # Build the transitive reverse dependency closure.
    while True:
        next_set = selected | {m["component"] for m in modules
                               if set(m["requires"]) & selected}
        if next_set == selected:
            return selected
        selected = next_set

def checked_flag(value):
    if not isinstance(value, str) or not value or any(
            token in value for token in ("&", "|", ">", "<", "\n", "\r", "%", "!", "^")):
        raise ValueError("unsafe compiler/linker flag in module registry")
    return value

def inspect_dll(path):
    data = path.read_bytes()
    if len(data) < 512 or data[:2] != b"MZ":
        raise ValueError("not a Windows PE DLL: " + str(path))
    off = struct.unpack_from("<I", data, 0x3c)[0]
    if off < 0x40 or off + 26 > len(data) or data[off:off+4] != b"PE\0\0":
        raise ValueError("invalid PE header: " + str(path))
    machine = struct.unpack_from("<H", data, off + 4)[0]
    characteristics = struct.unpack_from("<H", data, off + 22)[0]
    magic = struct.unpack_from("<H", data, off + 24)[0]
    if machine != 0x014c or magic != 0x010b or not (characteristics & 0x2000):
        raise ValueError("not PE32 x86 DLL: " + str(path))

def find_vcvars():
    pf = os.environ.get("ProgramFiles(x86)")
    if not pf:
        raise RuntimeError("Windows x86 Visual Studio tools are required")
    vswhere = Path(pf) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        raise RuntimeError("vswhere.exe is unavailable")
    install = subprocess.check_output([str(vswhere), "-latest", "-products", "*",
        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property", "installationPath"], text=True).strip()
    vcvars = Path(install) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
    if not vcvars.is_file():
        raise RuntimeError("vcvarsall.bat x86 is unavailable")
    return vcvars

def compile_module(m, runtime_file, vcvars, output_dir, verify_registered=True):
    recipe = m["build"]
    name = m["component"]
    filename = Path(runtime_file["path"]).name
    if Path(filename).suffix.lower() != ".dll":
        raise ValueError(name + ": runtime file is not DLL")
    # Compile each module in an isolated directory to avoid .obj collisions.
    folder = output_dir / re.sub(r"[^a-zA-Z0-9_-]", "_", name)
    folder.mkdir(parents=True, exist_ok=True)
    output = folder / filename
    sources = [str(repo_path(p)) for p in recipe["sources"]]
    if not all(Path(p).suffix.lower() in (".c", ".cc", ".cpp", ".cxx") for p in sources):
        raise ValueError(name + ": only declared C/C++ sources are supported")
    includes = []
    for p in recipe["include_dirs"]:
        full = repo_path(p)
        if not full.is_dir():
            raise ValueError(name + ": missing include directory " + p)
        includes.append("/I" + str(full))
    libs = [checked_flag(x) for x in recipe["libraries"]]
    flags = [checked_flag(x) for x in recipe["cflags"]]
    link_flags = [checked_flag(x) for x in recipe["ldflags"]]
    command = ["cl.exe", "/nologo", "/LD", "/O2", "/W4", "/Fo:" + str(folder) + "\\",
        "/Fe:" + str(output)] + includes + flags + sources + ["/link", "/MACHINE:X86",
        "/Brepro"] + link_flags + libs
    with tempfile.TemporaryDirectory(prefix="wow335-build-") as td:
        script = Path(td) / "build.cmd"
        # The script lives outside src/ and is never committed.
        script.write_text("@echo off\r\ncall \"" + str(vcvars) + "\" " + VCVARS_ARGS + "\r\n"
            + "if errorlevel 1 exit /b 1\r\n"
            + subprocess.list2cmdline(command) + "\r\n"
            + "exit /b %errorlevel%\r\n", encoding="utf-8")
        outcome = subprocess.run(["cmd.exe", "/d", "/c", str(script)], cwd=ROOT)
        if outcome.returncode:
            raise RuntimeError(name + ": x86 DLL compilation failed")
    inspect_dll(output)
    actual = sha256_file(output)
    if verify_registered:
        if actual != runtime_file["sha256"]:
            raise ValueError(name + ": compiled DLL SHA256 differs from registered runtime: "
                + actual + " != " + runtime_file["sha256"] + "; do not package stale binaries")
        if sha256_file(repo_path(runtime_file["path"])) != actual:
            raise ValueError(name + ": registered game DLL changed during build")
    return {"component": name, "source_sha256": {p: sha256_file(repo_path(p))
            for p in recipe["sources"]}, "binary_sha256": actual,
            "binary_name": filename, "verification": ("EXACT_PE32_X86_REBUILD_PASS"
                if verify_registered else "PE32_X86_BUILD_AWAITING_REGISTRATION")}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--report", default="dist/native_build.json")
    args = parser.parse_args()
    try:
        runtime = load_json(ROOT / "runtime/current.json")
        registry = load_json(ROOT / "runtime/module_registry.json")
        errors = validate(runtime, registry)
        if errors:
            raise ValueError("; ".join(errors))
        active = {row["component"]: row for row in runtime["files"]
                  if row.get("kind") == "dll"}
        if not active:
            raise ValueError("NO_ACTIVE_DLL: a compiler smoke test is not a game build")
        selected = select_modules(registry, None if args.all else args.base)
        vcvars = find_vcvars() if selected else None
        output_dir = ROOT / "dist" / "native"
        results = []
        for m in registry["modules"]:
            if m["component"] in selected:
                results.append(compile_module(m, active[m["component"]],
                                              vcvars, output_dir))
        report = {"schema_version": 1, "project": "335", "build": 12340,
                  "git_sha": os.environ.get("GITHUB_SHA") or subprocess.check_output(
                      ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  "selected_components": sorted(selected), "results": results,
                  "toolchain": {"vcvars_args": VCVARS_ARGS, "vc_version": PINNED_VC_VERSION,
                                "windows_sdk": PINNED_WINDOWS_SDK}}
        dest = repo_path(args.report)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print("ACTIVE_DLL_BUILD: PASS; rebuilt", len(results),
              "of", len(active), "registered modules")
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print("ACTIVE_DLL_BUILD: FAIL", exc)
        return 1

if __name__ == "__main__":
    sys.exit(main())
