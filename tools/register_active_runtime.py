"""Safely register all already-declared native TEST DLLs on work.

This is not a packaging shortcut. Compiled bytes first update runtime/current.json
in a single commit and then MUST survive the existing independent exact-SHA
rebuild and final-package gates. Refuse bootstrap, STABLE and undeclared DLLs.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

from manifest_common import ROOT, load_json, repo_path, sha256_file
from build_active import compile_module, find_vcvars
from verify_module_registry import validate


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def verify(*args):
    subprocess.run([sys.executable, *args], cwd=ROOT, check=True, timeout=300)


def register():
    initial = os.environ.get("GITHUB_SHA", "")
    if (os.environ.get("GITHUB_ACTIONS") != "true" or
        os.environ.get("GITHUB_REF_NAME") != "work" or
        os.environ.get("RUNNER_OS") != "Windows" or
        re.fullmatch(r"[0-9a-f]{40}", initial) is None or
        git("rev-parse", "HEAD") != initial or git("status", "--porcelain")):
        raise RuntimeError("native registration requires clean, exact work HEAD on Windows Actions")

    current = load_json(ROOT / "CURRENT.json")
    runtime = load_json(ROOT / "runtime/current.json")
    registry = load_json(ROOT / "runtime/module_registry.json")
    selected = load_json(ROOT / "runtime/client_exe_target.json")
    if (current.get("status") == "stable" or current.get("stable_baseline") or
        runtime.get("state") != "test-candidate" or
        selected.get("target_build") != 12340 or
        selected.get("sha256") != sha256_file(ROOT / "Wow.exe")):
        raise RuntimeError("only existing 12340 TEST runtime may be registered")
    errors = validate(runtime, registry)
    if errors:
        raise RuntimeError("invalid module/dependency contract: " + "; ".join(errors))
    files = runtime.get("files", [])
    exes = [item for item in files if item.get("kind") == "exe"]
    dlls = [item for item in files if item.get("kind") == "dll"]
    if (len(exes) != 1 or len(dlls) == 0 or
        exes[0].get("sha256") != selected["sha256"] or
        len(dlls) != len(registry["modules"])):
        raise RuntimeError("registered TEST needs one pinned EXE and exactly declared DLL set")

    # Verify previous exact bytes and build contract before replacing anything.
    verify("tools/verify_repo.py")
    verify("tools/verify_current.py")
    verify("tools/verify_module_registry.py")
    verify("-m", "unittest", "discover", "-s", "tests", "-v")
    active = {item["component"]: item for item in dlls}
    outdir = ROOT / "dist" / "native"
    vcvars = find_vcvars()
    changed = []
    for module in registry["modules"]:
        item = active[module["component"]]
        built = compile_module(module, item, vcvars, outdir, verify_registered=False)
        binary = outdir / re.sub(r"[^a-zA-Z0-9_-]", "_", module["component"]) / built["binary_name"]
        if sha256_file(binary) != built["binary_sha256"]:
            raise RuntimeError("compiled output changed unexpectedly: " + module["component"])
        if built["binary_sha256"] != item["sha256"]:
            changed.append((item, binary, built))
    if not changed:
        print("NATIVE_REGISTER: SKIP; all registered module DLLs reproduce exactly")
        return

    # All changed DLLs and the manifest become ONE commit. Never publish a
    # partial dependency set or modify arbitrary existing client files.
    staged = []
    for item, binary, built in changed:
        dest = repo_path(item["path"])
        if dest.suffix.lower() != ".dll" or not str(dest).startswith(str(ROOT / "runtime" / "")):
            raise RuntimeError("DLL destination is not under runtime/: " + str(dest))
        dest.write_bytes(binary.read_bytes())
        if sha256_file(dest) != built["binary_sha256"]:
            raise RuntimeError("staged DLL hash mismatch: " + item["component"])
        base_version = item["version"].split("+sha.")[0]
        item["version"] = base_version + "+sha." + built["binary_sha256"][:12]
        item["sha256"] = built["binary_sha256"]
        staged.append(item["path"])
    (ROOT / "runtime/current.json").write_text(
        json.dumps(runtime, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    staged.append("runtime/current.json")

    # Independent native rebuild retains the strict SHA gate; CI fails closed
    # if the toolchain emits different bytes on its second invocation.
    verify("tools/verify_repo.py")
    verify("tools/verify_current.py")
    verify("tools/verify_module_registry.py")
    verify("tools/build_active.py", "--all", "--report", "dist/native_build.json")
    verify("tools/verify_current.py")
    actual = set(git("diff", "--name-only").splitlines())
    if actual != set(staged) or git("rev-parse", "HEAD") != initial:
        raise RuntimeError("unexpected tracked changes during native registration: " + repr(actual))
    subprocess.run(["git", "fetch", "origin", "work"], cwd=ROOT, check=True, timeout=120)
    if git("rev-parse", "FETCH_HEAD") != initial:
        raise RuntimeError("work advanced; refusing stale native registration")
    subprocess.run(["git", "config", "user.name", "wow335-native-build"], cwd=ROOT, check=True)
    subprocess.run(["git", "config", "user.email",
                    "41898282+github-actions[bot]@users.noreply.github.com"], cwd=ROOT, check=True)
    subprocess.run(["git", "add", "--", *staged], cwd=ROOT, check=True)
    subprocess.run(["git", "commit", "-m",
                    "build(runtime): register independently verified TEST x86 module set"], cwd=ROOT, check=True)
    committed = git("rev-parse", "HEAD")
    subprocess.run(["git", "push", "origin", "HEAD:refs/heads/work"],
                   cwd=ROOT, check=True, timeout=120)
    print("NATIVE_REGISTERED_WORK_SHA:", committed)
    for item, _, built in changed:
        print("NATIVE_REGISTERED_DLL:", item["component"], built["binary_sha256"])


if __name__ == "__main__":
    register()
