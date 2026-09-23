"""Publish exactly rebuilt 12340 x86 AutoLoot as a registered TEST runtime.

Runs only on authorized GitHub Actions Windows x86 on work. It first compiles
the real native DLL from canonical sources, records the ACTUAL binary checksum,
then subjects those exact bytes to the unchanged module/runtime/rebuild gates
before committing. A failed gate leaves work untouched. Never write main.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from manifest_common import ROOT, load_json, repo_path, sha256_file
from build_active import checked_flag, find_vcvars, inspect_dll
from native_toolchain import VCVARS_ARGS

COMPONENT = "AutoLoot"
BINARY = "runtime/AutoLoot335.dll"
SOURCES = [
    "src/AutoLoot/autoloot_core.c",
    "src/AutoLoot/autoloot_core.h",
    "src/AutoLoot/autoloot_12340_adapter.c",
    "src/AutoLoot/autoloot_12340_adapter.h",
    "src/AutoLoot/autoloot_win32_host.c",
    "src/AutoLoot/autoloot_win32_launcher.c",
]
COMPILE_SOURCES = [s for s in SOURCES if s.endswith(".c") and
                   not s.endswith("autoloot_win32_launcher.c")]
TARGET_SHA256 = "2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d"


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def run(*args: str) -> None:
    subprocess.run([sys.executable, *args], cwd=ROOT, check=True, timeout=240)


def execute() -> None:
    initial = os.environ.get("GITHUB_SHA", "")
    if (os.environ.get("GITHUB_ACTIONS") != "true" or
        os.environ.get("GITHUB_REF_NAME") != "work" or
        os.environ.get("RUNNER_OS") != "Windows" or
        len(initial) != 40 or git("rev-parse", "HEAD") != initial):
        raise RuntimeError("refusing unsigned or non-work binary publication")
    target = load_json(ROOT / "runtime/client_exe_target.json")
    if (target.get("sha256") != TARGET_SHA256 or
        sha256_file(ROOT / "Wow.exe") != TARGET_SHA256):
        raise RuntimeError("selected exact 12340 Wow.exe is unavailable")
    manifest = load_json(ROOT / "runtime/current.json")
    registry = load_json(ROOT / "runtime/module_registry.json")
    if (manifest.get("state") != "empty" or manifest.get("files") or registry.get("modules")):
        raise RuntimeError("runtime already registered: refuse to overwrite/republish it")

    # Match tools/build_active.py's invocation, object directory, optimization
    # and linker flags byte-for-byte; its strict unchanged rebuild is run next.
    folder = ROOT / "dist" / "native" / COMPONENT
    folder.mkdir(parents=True, exist_ok=True)
    out = folder / "AutoLoot335.dll"
    cmd = (["cl.exe", "/nologo", "/LD", "/O2", "/W4",
             "/Fo:" + str(folder) + "\\", "/Fe:" + str(out),
             "/I" + str(ROOT / "src" / "AutoLoot"), "/TC", "/Brepro"] +
           [str(repo_path(p)) for p in COMPILE_SOURCES] +
           ["/link", "/MACHINE:X86", "/Brepro", "Advapi32.lib", "User32.lib"])
    with tempfile.TemporaryDirectory(prefix="wow335-runtime-publish-") as td:
        script = Path(td) / "build.cmd"
        script.write_text(
            "@echo off\r\ncall \"" + str(find_vcvars()) + "\" " + VCVARS_ARGS + "\r\n"
            "if errorlevel 1 exit /b 1\r\n" +
            subprocess.list2cmdline(cmd) + "\r\nexit /b %errorlevel%\r\n",
            encoding="utf-8",
        )
        subprocess.run(["cmd.exe", "/d", "/c", str(script)], cwd=ROOT, check=True, timeout=240)
    inspect_dll(out)
    binary_digest = sha256_file(out)
    binary_path = ROOT / BINARY
    binary_path.parent.mkdir(parents=True, exist_ok=True)
    binary_path.write_bytes(out.read_bytes())
    if sha256_file(binary_path) != binary_digest:
        raise RuntimeError("output file SHA differs from x86 native build")

    # Only ONE registered exe and ONE registered DLL; no unaccepted modules.
    manifest["state"] = "test-candidate"
    manifest["release_id"] = "work-autoloot-12340"
    manifest["files"] = [
        {
            "component": "Client12340", "path": "Wow.exe",
            "version": "3.3.5.12340", "sha256": TARGET_SHA256,
            "arch": "x86", "canonical_source": "runtime/client_exe_target.json",
            "depends_on": [], "kind": "exe",
        },
        {
            "component": COMPONENT, "path": BINARY,
            "version": "1.0.0-test", "sha256": binary_digest,
            "arch": "x86", "canonical_source": "src/AutoLoot/autoloot_win32_host.c",
            "depends_on": [], "kind": "dll",
        },
    ]
    manifest["compatibility_sets"] = [
        {"id": "client12340-autoloot", "components": ["Client12340", COMPONENT]}
    ]
    registry["modules"] = [
        {
            "component": COMPONENT,
            "sources": SOURCES,
            "requires": [],
            "resources": [
                {"id": "wow12340:object-manager", "mode": "observe"},
                {"id": "wow12340:0x00731260-unit-interact", "mode": "exclusive"},
                {"id": "wow12340:0x00819210-framescript-execute", "mode": "exclusive"},
                {"id": "win32:WH_GETMESSAGE", "mode": "exclusive"},
                {"id": "logical:loot-ui", "mode": "exclusive"},
                {"id": "logical:interaction", "mode": "exclusive"},
                {"id": "logical:input", "mode": "observe"},
            ],
            "build": {
                "toolchain": "msvc_x86", "sources": COMPILE_SOURCES,
                "include_dirs": ["src/AutoLoot"],
                "libraries": ["Advapi32.lib", "User32.lib"],
                "cflags": ["/TC", "/Brepro"], "ldflags": [],
            },
        }
    ]
    idx = load_json(ROOT / "AI_INDEX.json")
    if idx.get("modules"):
        raise RuntimeError("AI_INDEX already has modules: selective promotion only")
    idx["modules"] = [
        {"component": COMPONENT,
         "source": "src/AutoLoot/autoloot_win32_host.c",
         "docs": "src/AutoLoot/NATIVE_ADAPTER.md"}
    ]
    # Do not claim stable release or in-game acceptance for new SHA.
    for path, value in [
        (ROOT / "runtime/current.json", manifest),
        (ROOT / "runtime/module_registry.json", registry),
        (ROOT / "AI_INDEX.json", idx),
    ]:
        path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    run("tools/verify_repo.py")
    run("tools/verify_current.py")
    run("tools/verify_module_registry.py")
    run("-m", "unittest", "discover", "-s", "tests", "-v")
    # Crucial: identical real DLL rebuild with existing fail-closed verifier.
    run("tools/build_active.py", "--all", "--report", "dist/native_build.json")
    run("tools/verify_current.py")

    if git("rev-parse", "HEAD") != initial:
        raise RuntimeError("HEAD changed while preparing the binary")
    subprocess.run(["git", "fetch", "origin", "work"], cwd=ROOT, check=True, timeout=120)
    if git("rev-parse", "FETCH_HEAD") != initial:
        raise RuntimeError("remote work advanced: refuse to publish stale build")
    subprocess.run(["git", "config", "user.name", "wow335-native-build"],
                   cwd=ROOT, check=True)
    subprocess.run(["git", "config", "user.email",
                    "41898282+github-actions[bot]@users.noreply.github.com"],
                   cwd=ROOT, check=True)
    files = [BINARY, "runtime/current.json",
             "runtime/module_registry.json", "AI_INDEX.json"]
    subprocess.run(["git", "add", "--", *files], cwd=ROOT, check=True)
    subprocess.run(["git", "commit", "-m",
                    "build(autoloot): register exact rebuilt PE32 x86 DLL on TEST work"],
                   cwd=ROOT, check=True)
    committed = git("rev-parse", "HEAD")
    subprocess.run(["git", "push", "origin", "HEAD:refs/heads/work"],
                   cwd=ROOT, check=True, timeout=120)
    print("AUTOLOOT_REGISTERED_WORK_SHA:", committed)
    print("AUTOLOOT_REGISTERED_DLL_SHA256:", binary_digest)


def refresh_registered_runtime() -> None:
    """Re-register the actual PE32 x86 DLL when a different MSVC toolchain
    emits different bytes. Never weaken the exact rebuild/package gates.
    Only the isolated, already registered AutoLoot TEST component is writable.
    """
    initial = os.environ.get("GITHUB_SHA", "")
    if (os.environ.get("GITHUB_ACTIONS") != "true" or
        os.environ.get("GITHUB_REF_NAME") != "work" or
        os.environ.get("RUNNER_OS") != "Windows" or
        len(initial) != 40 or git("rev-parse", "HEAD") != initial or
        git("status", "--porcelain")):
        raise RuntimeError("refresh requires clean work HEAD on a Windows Actions runner")
    manifest = load_json(ROOT / "runtime/current.json")
    registry = load_json(ROOT / "runtime/module_registry.json")
    target = load_json(ROOT / "runtime/client_exe_target.json")
    files = manifest.get("files", [])
    modules = registry.get("modules", [])
    if (manifest.get("state") != "test-candidate" or
        len(files) != 2 or len(modules) != 1 or
        files[0].get("kind") != "exe" or
        files[0].get("path") != "Wow.exe" or
        files[0].get("sha256") != TARGET_SHA256 or
        files[1].get("kind") != "dll" or
        files[1].get("component") != COMPONENT or
        files[1].get("path") != BINARY or
        modules[0].get("component") != COMPONENT or
        modules[0].get("requires") != [] or
        target.get("sha256") != TARGET_SHA256 or
        sha256_file(ROOT / "Wow.exe") != TARGET_SHA256):
        raise RuntimeError("refresh is limited to the exact existing 12340 TEST AutoLoot stack")

    # Inspect and check the registered bytes BEFORE writing a replacement.
    run("tools/verify_repo.py")
    run("tools/verify_current.py")
    run("tools/verify_module_registry.py")
    run("-m", "unittest", "discover", "-s", "tests", "-v")
    from build_active import compile_module
    recipe = modules[0]["build"]
    if (recipe.get("toolchain") != "msvc_x86" or
        recipe.get("sources") != COMPILE_SOURCES):
        raise RuntimeError("unexpected native source/build ownership contract")
    folder = ROOT / "dist" / "native" / COMPONENT
    folder.mkdir(parents=True, exist_ok=True)
    out = folder / "AutoLoot335.dll"
    sources = [str(repo_path(source)) for source in recipe["sources"]]
    includes = ["/I" + str(repo_path(path)) for path in recipe["include_dirs"]]
    flags = [checked_flag(flag) for flag in recipe["cflags"]]
    libraries = [checked_flag(lib) for lib in recipe["libraries"]]
    linker = [checked_flag(flag) for flag in recipe["ldflags"]]
    cmd = (["cl.exe", "/nologo", "/LD", "/O2", "/W4",
            "/Fo:" + str(folder) + "\\", "/Fe:" + str(out)] +
           includes + flags + sources + ["/link", "/MACHINE:X86", "/Brepro"] +
           linker + libraries)
    with tempfile.TemporaryDirectory(prefix="wow335-runtime-refresh-") as td:
        script = Path(td) / "build.cmd"
        script.write_text(
            "@echo off\r\ncall \"" + str(find_vcvars()) + "\" " + VCVARS_ARGS + "\r\n"
            "if errorlevel 1 exit /b 1\r\n" +
            subprocess.list2cmdline(cmd) + "\r\nexit /b %errorlevel%\r\n",
            encoding="utf-8",
        )
        subprocess.run(["cmd.exe", "/d", "/c", str(script)], cwd=ROOT,
                       check=True, timeout=240)
    inspect_dll(out)
    built_digest = sha256_file(out)
    old_digest = files[1]["sha256"]
    if built_digest == old_digest:
        if sha256_file(ROOT / BINARY) != old_digest:
            raise RuntimeError("existing registered DLL bytes unexpectedly changed")
        print("AUTOLOOT_REFRESH: SKIP; existing native bytes reproduce exactly")
        return

    (ROOT / BINARY).write_bytes(out.read_bytes())
    if sha256_file(ROOT / BINARY) != built_digest:
        raise RuntimeError("staged rebuilt DLL differs from measured compiler output")
    files[1]["sha256"] = built_digest
    files[1]["version"] = "1.0.2-test"
    manifest["release_id"] = "work-autoloot-x86-rebuild-12340"
    (ROOT / "runtime/current.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8")

    # The normal, unmodified build_active verifier recompiles the DLL independently.
    # If a second compile differs, refuse publication rather than weakening SHA gates.
    run("tools/verify_repo.py")
    run("tools/verify_current.py")
    run("tools/verify_module_registry.py")
    run("-m", "unittest", "discover", "-s", "tests", "-v")
    run("tools/build_active.py", "--all", "--report", "dist/native_build.json")
    run("tools/verify_current.py")
    if git("rev-parse", "HEAD") != initial:
        raise RuntimeError("HEAD changed during native runtime refresh")
    changed = set(git("diff", "--name-only").splitlines())
    if changed != {BINARY, "runtime/current.json"}:
        raise RuntimeError("native refresh unexpectedly changed other tracked files: " +
                           repr(sorted(changed)))
    subprocess.run(["git", "fetch", "origin", "work"], cwd=ROOT,
                   check=True, timeout=120)
    if git("rev-parse", "FETCH_HEAD") != initial:
        raise RuntimeError("remote work advanced; refuse stale native binary update")
    subprocess.run(["git", "config", "user.name", "wow335-native-build"],
                   cwd=ROOT, check=True)
    subprocess.run(["git", "config", "user.email",
                    "41898282+github-actions[bot]@users.noreply.github.com"],
                   cwd=ROOT, check=True)
    subprocess.run(["git", "add", "--", BINARY, "runtime/current.json"],
                   cwd=ROOT, check=True)
    subprocess.run(["git", "commit", "-m",
                    "build(autoloot): register exact independently rebuilt 12340 x86 DLL"],
                   cwd=ROOT, check=True)
    committed = git("rev-parse", "HEAD")
    subprocess.run(["git", "push", "origin", "HEAD:refs/heads/work"],
                   cwd=ROOT, check=True, timeout=120)
    print("AUTOLOOT_REFRESHED_WORK_SHA:", committed)
    print("AUTOLOOT_REFRESHED_DLL_SHA256:", built_digest)


if __name__ == "__main__":
    if load_json(ROOT / "runtime/current.json").get("state") == "empty":
        execute()
    else:
        refresh_registered_runtime()
