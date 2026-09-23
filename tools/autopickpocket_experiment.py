"""Isolate a non-runnable AutoPickPocket core experiment from game packaging.

Full game candidate build remains mandatory whenever active game source,
runtime ownership or native build infrastructure changes. This mode NEVER
produces a game DLL, a game ZIP or FINAL_PACKAGE: PASS.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BRANCH = "feature/autopickpocket-12340"
SENSITIVE = {
    "runtime/current.json",
    "runtime/module_registry.json",
    "runtime/client_exe_target.json",
    "tools/build_active.py",
    "tools/package_candidate.py",
    "tools/verify_candidate_package.py",
    "tools/verify_module_registry.py",
    "tools/verify_current.py",
    "tools/verify_repo.py",
    ".github/workflows/verify.yml",
}

def core_only(branch: str, changed: set[str], manifest: dict, registry: dict) -> bool:
    if branch != BRANCH or not changed:
        return False
    if any(row.get("component") == "AutoPickPocket" for row in manifest.get("files", [])):
        return False
    if any(m.get("component") == "AutoPickPocket" for m in registry.get("modules", [])):
        return False
    active_sources = {
        source for module in registry.get("modules", [])
        for source in module.get("sources", [])
    }
    # A change to an active module or gate always requires the full strict build.
    if changed & (SENSITIVE | active_sources):
        return False
    # Also forbid changes to any active module's parent directory: catches new
    # includes/dependencies that may not yet be registered as source entries.
    active_dirs = {str(Path(p).parent).replace("\\", "/") + "/" for p in active_sources}
    if any(path.startswith(tuple(active_dirs)) for path in changed):
        return False
    if not any(p.startswith("src/AutoPickPocket/") for p in changed):
        return False
    return all(
        path.startswith(("src/AutoPickPocket/", "tests/autopickpocket_",
                         "tests/test_autopickpocket_"))
        or path in {"tools/updater/UpdaterIssueReportFeature.cs",
                    ".github/workflows/build_work_candidate.yml",
                    "tools/autopickpocket_experiment.py",
                    "runtime/ai_experiments.json",
                    ".github/workflows/audit_autopickpocket_abi.yml"}
        for path in changed
    )

def route(output: str | None) -> None:
    from manifest_common import load_json
    branch = os.environ.get("GITHUB_REF_NAME", "")
    # Failure to resolve branch ancestry must NOT accidentally skip game build.
    changed: set[str] = set()
    try:
        base = subprocess.check_output(
            ["git", "merge-base", "HEAD", "origin/work"], cwd=ROOT,
            text=True, stderr=subprocess.DEVNULL).strip()
        if base:
            changed = set(subprocess.check_output(
                ["git", "diff", "--name-only", base, "HEAD", "--"], cwd=ROOT,
                text=True).splitlines())
    except (OSError, subprocess.CalledProcessError):
        pass
    only = core_only(branch, changed,
                     load_json(ROOT / "runtime/current.json"),
                     load_json(ROOT / "runtime/module_registry.json"))
    if output:
        with open(output, "a", encoding="utf-8") as f:
            f.write("core_only=" + ("true" if only else "false") + "\n")
    if only:
        print("AUTOPICKPOCKET_CORE_ONLY: no game DLL registered; "
              "build x86 core tests; NO GAME PACKAGE.")
    else:
        print("FULL_GAME_BUILD_REQUIRED: strict registered DLL rebuild unchanged.")

def windows_core_build(report: str) -> None:
    if os.name != "nt":
        raise RuntimeError("Windows MSVC x86 is required")
    from build_active import find_vcvars
    from manifest_common import pe_machine, repo_path, sha256_file
    from build_active import inspect_dll
    vcvars = find_vcvars()
    src = repo_path("src/AutoPickPocket/autopickpocket_core.c")
    test = repo_path("tests/autopickpocket_core_test.c")
    out = repo_path("dist/pp_core_experiment")
    out.mkdir(parents=True, exist_ok=True)
    executable = out / "autopickpocket_core_test.exe"
    with tempfile.TemporaryDirectory(prefix="pp335-ci-") as temp:
        cmd = Path(temp) / "build.cmd"
        cmd.write_text(
            '@echo off\r\ncall "' + str(vcvars) + '" x86\r\n'
            'if errorlevel 1 exit /b 1\r\n'
            'cl.exe /nologo /TC /O2 /W4 /WX /Brepro '
            '/Fe:"' + str(executable) + '" "' + str(src) + '" "' +
            str(test) + '" /link /MACHINE:X86 /Brepro\r\n'
            'if errorlevel 1 exit /b 1\r\n'
            '"' + str(executable) + '"\r\n'
            'exit /b %errorlevel%\r\n', encoding="utf-8")
        result = subprocess.run(["cmd.exe", "/d", "/c", str(cmd)], cwd=out)
        if result.returncode:
            raise RuntimeError("AutoPickPocket native x86 core tests failed")
    if pe_machine(executable) != 0x014c:
        raise RuntimeError("AutoPickPocket test executable is not Windows x86")
    adapter = repo_path("src/AutoPickPocket/autopickpocket_12340_adapter.c")
    adapter_test = repo_path("tests/autopickpocket_12340_adapter_test.c")
    dll_host = repo_path("src/AutoPickPocket/autopickpocket_win32_host.c")
    native_test = out / "autopickpocket_adapter_test.exe"
    dll = out / "AutoPickPocket335_ADAPTER_ISOLATED.dll"
    with tempfile.TemporaryDirectory(prefix="pp335-adapter-") as temp:
        cmd = Path(temp) / "build.cmd"
        cmd.write_text(
            '@echo off\r\ncall "' + str(vcvars) + '" x86\r\n'
            'if errorlevel 1 exit /b 1\r\n'
            'cl.exe /nologo /TC /O2 /W4 /WX /Brepro '
            '/Fe:"' + str(native_test) +
            '" "' + str(src) + '" "' + str(adapter) + '" "' +
            str(adapter_test) + '" /link /MACHINE:X86 /Brepro\r\n'
            'if errorlevel 1 exit /b 1\r\n'
            '"' + str(native_test) + '"\r\n'
            'if errorlevel 1 exit /b 1\r\n'
            'cl.exe /nologo /LD /TC /O2 /W4 /WX /Brepro '
            '/Fe:"' + str(dll) +
            '" "' + str(src) + '" "' + str(adapter) + '" "' +
            str(dll_host) + '" /link /MACHINE:X86 /Brepro Advapi32.lib User32.lib\r\n'
            'exit /b %errorlevel%\r\n', encoding="utf-8")
        compile = subprocess.run(["cmd.exe", "/d", "/c", str(cmd)], cwd=out)
        if compile.returncode:
            raise RuntimeError("AutoPickPocket native adapter mock or isolated x86 DLL build failed")
    if pe_machine(native_test) != 0x014c:
        raise RuntimeError("AutoPickPocket adapter test is not Windows x86")
    inspect_dll(dll)
    sha = os.environ.get("GITHUB_SHA", "")
    data = {
        "schema_version": 1,
        "branch": os.environ.get("GITHUB_REF_NAME", ""),
        "git_sha": sha,
        "test_executable_sha256": sha256_file(executable),
        "adapter_test_executable_sha256": sha256_file(native_test),
        "isolated_adapter_dll_sha256": sha256_file(dll),
        "result": "PORTABLE_CORE_AND_NATIVE_ADAPTER_MOCK_X86_PASS",
        "isolated_adapter_compiled_pe32_x86": True,
        "game_dll_registered": False,
        "game_ready": False,
        "final_package": False,
        "reason": "NPC eligibility, learned-spell and cast-result policies plus shared game-thread loader not integrated"
    }
    dest = repo_path(report)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print("AUTOPICKPOCKET_ADAPTER_X86: PASS; isolated PE32 adapter and mock tests; NO GAME PACKAGE.")

def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("action", choices=("route", "build"))
    p.add_argument("--output")
    p.add_argument("--report", default="dist/pp_core_experiment.json")
    args=p.parse_args()
    try:
        if args.action=="route": route(args.output)
        else: windows_core_build(args.report)
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as exc:
        print("AUTOPICKPOCKET_EXPERIMENT: FAIL", exc)
        return 1

if __name__ == "__main__":
    sys.exit(main())
