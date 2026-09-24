"""Stage real PlayerESP x86 DLL into isolated feature TEST runtime.

Never run on main/work. The final package must still pass build_work_candidate
at the exact new commit SHA; this stage does not publish a game package.
"""
from __future__ import annotations
import argparse
import copy
import json
import os
import shutil
import sys

from build_active import compile_module, find_vcvars
from manifest_common import ROOT, load_json, sha256_file
from verify_module_registry import validate

BRANCH = "feature/player-esp-12340"
SOURCE = [
    "src/PlayerESP/player_esp_core.c",
    "src/PlayerESP/player_esp_core.h",
    "src/PlayerESP/player_esp_scanner.c",
    "src/PlayerESP/player_esp_scanner.h",
    "src/PlayerESP112Port/esp112_geometry.c",
    "src/PlayerESP112Port/esp112_geometry.h",
    "src/PlayerESP112Port/esp112_overlay.c",
    "src/PlayerESP112Port/esp112_overlay.h",
    "src/PlayerESP112Port/esp112_slots.c",
    "src/PlayerESP112Port/esp112_slots.h",
    "src/PlayerESP112Port/esp112_host335.c",
]
BUILD = [name for name in SOURCE if name.endswith(".c")]

def esp_contract():
    return {
        "component": "PlayerESP",
        "sources": SOURCE,
        "requires": [],
        "resources": [
            {"id": "wow12340:object-manager", "mode": "observe"},
            {"id": "wow12340:0x004f6d20-native-w2s", "mode": "observe"},
            {"id": "wow12340:0x0047bff0-native-ddc", "mode": "observe"},
            {"id": "win32:layered-label-overlay", "mode": "exclusive"},
            {"id": "logical:render", "mode": "exclusive"},
            {"id": "logical:input", "mode": "observe"},
            {"id": "win32:WH_GETMESSAGE", "mode": "chain",
             "arbitrator": "Loader"},
            {"id": "win32:WH_CALLWNDPROC", "mode": "chain",
             "arbitrator": "Loader"},
        ],
        "build": {
            "toolchain": "msvc_x86",
            "sources": BUILD,
            "include_dirs": ["src/PlayerESP", "src/PlayerESP112Port"],
            "libraries": ["Advapi32.lib", "User32.lib", "Gdi32.lib"],
            "cflags": ["/TC", "/Brepro"],
            "ldflags": [],
        },
    }

def prepare_registration(runtime, registry, index, dll_sha, loot_sha=None):
    """Only the expected unmodified feature baseline may be staged."""
    runtime, registry, index = [copy.deepcopy(x) for x in
                                (runtime, registry, index)]
    if (not isinstance(dll_sha, str) or len(dll_sha) != 64 or
            any(ch not in "0123456789abcdef" for ch in dll_sha)):
        raise ValueError("unverified DLL identity")
    if loot_sha is not None:
        if not isinstance(loot_sha,str) or len(loot_sha)!=64 or any(ch not in "0123456789abcdef" for ch in loot_sha):
            raise ValueError("invalid AutoLoot x86 DLL identity")
        runtime["files"][1]["sha256"]=loot_sha
        runtime["files"][1]["version"]="1.0.2-esp-ui-gate-test"
    current = [entry["component"] for entry in runtime["files"]]
    if current == ["Client12340", "AutoLoot", "PlayerESP"]:
        if ([m["component"] for m in registry["modules"]] != ["AutoLoot","PlayerESP"] or
            [m["component"] for m in index["modules"]] != ["AutoLoot","PlayerESP"] or
            runtime["files"][2].get("path") != "runtime/PlayerESP335.dll" or
            runtime["files"][2].get("kind") != "dll" or
            runtime["files"][2].get("canonical_source") not in (
                "src/PlayerESP/player_esp_win32_host.c",
                "src/PlayerESP112Port/esp112_host335.c") or
            registry["modules"][1].get("component") != "PlayerESP"):
            raise ValueError("existing ESP registration diverged; refusing overwrite")
        runtime["files"][2]["sha256"] = dll_sha
        runtime["files"][2]["version"] = "0.4.0-112-gdi-rebuild-test"
        runtime["files"][2]["canonical_source"]="src/PlayerESP112Port/esp112_host335.c"
        runtime["files"][2]["depends_on"]=[]
        runtime["release_id"] = "feature-player-esp-12340-112-gdi-rebuild-test"
        registry["modules"][1] = esp_contract()
        index["modules"][1]["source"]="src/PlayerESP112Port/esp112_host335.c"
        index["modules"][1]["docs"]="src/PlayerESP112Port/README.md"
        return runtime, registry, index
    if current != ["Client12340", "AutoLoot"]:
        raise ValueError("unexpected runtime; refusing to overwrite")
    if [m["component"] for m in registry["modules"]] != ["AutoLoot"]:
        raise ValueError("unexpected registry; refusing to overwrite")
    if [m["component"] for m in index["modules"]] != ["AutoLoot"]:
        raise ValueError("unexpected AI_INDEX; refusing to overwrite")
    if runtime.get("target", {}).get("build") != 12340:
        raise ValueError("not the exact 12340 target")
    hook_ids = {"win32:WH_GETMESSAGE", "win32:WH_CALLWNDPROC"}
    found = set()
    for resource in registry["modules"][0]["resources"]:
        if resource.get("id") in hook_ids:
            if resource.get("mode") != "exclusive":
                raise ValueError("unexpected existing hook ownership")
            found.add(resource["id"])
            resource["mode"] = "chain"
            resource["arbitrator"] = "Loader"
    if found != hook_ids:
        raise ValueError("original Loader hook contract missing")
    registry["modules"].append(esp_contract())
    runtime["files"].append({
        "component": "PlayerESP", "path": "runtime/PlayerESP335.dll",
        "version": "0.4.0-112-gdi-rebuild-test",
        "sha256": dll_sha, "arch": "x86",
        "canonical_source": "src/PlayerESP112Port/esp112_host335.c",
        "depends_on": [], "kind": "dll",
    })
    runtime["release_id"] = "feature-player-esp-12340-112-gdi-rebuild-test"
    runtime["compatibility_sets"] = [{
        "id": "client12340-autoloot-player-esp-test",
        "components": ["Client12340", "AutoLoot", "PlayerESP"],
    }]
    index["modules"].append({
        "component": "PlayerESP",
        "source": "src/PlayerESP112Port/esp112_host335.c",
        "docs": "src/PlayerESP112Port/README.md",
    })
    return runtime, registry, index

def write_json(path, data):
    (ROOT / path).write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", action="store_true", required=True)
    parser.parse_args()
    if os.name != "nt" or os.getenv("GITHUB_REF_NAME") != BRANCH:
        raise ValueError("only Windows feature/player-esp-12340 may stage")
    runtime = load_json(ROOT / "runtime/current.json")
    registry = load_json(ROOT / "runtime/module_registry.json")
    index = load_json(ROOT / "AI_INDEX.json")
    destination = ROOT / "runtime/PlayerESP335.dll"
    active = next((entry for entry in runtime["files"] if entry.get("component") == "PlayerESP"), None)
    if active:
        if not destination.is_file() or sha256_file(destination) != active["sha256"]:
            raise ValueError("existing ESP binary differs from registered runtime")
    elif destination.exists():
        raise ValueError("unregistered existing ESP binary; refusing overwrite")
    folder = ROOT / "dist/esp-stage"
    vcvars=find_vcvars()
    loot_compiled=compile_module(registry["modules"][0],runtime["files"][1],
                                 vcvars,folder,verify_registered=False)
    loot_binary=folder/"AutoLoot"/"AutoLoot335.dll"
    if sha256_file(loot_binary)!=loot_compiled["binary_sha256"]:
        raise ValueError("compiled AutoLoot bridge not exact PE32 x86")
    compiled = compile_module(
        esp_contract(), {"path": "runtime/PlayerESP335.dll"},
        vcvars, folder, verify_registered=False)
    binary = folder / "PlayerESP/PlayerESP335.dll"
    if (compiled.get("verification") !=
            "PE32_X86_BUILD_AWAITING_REGISTRATION" or
            sha256_file(binary) != compiled["binary_sha256"]):
        raise ValueError("native x86 build not verified")
    manifest, owners, idx = prepare_registration(
        runtime, registry, index, compiled["binary_sha256"],
        loot_compiled["binary_sha256"])
    shutil.copyfile(loot_binary, ROOT/"runtime/AutoLoot335.dll")
    if sha256_file(ROOT/"runtime/AutoLoot335.dll")!=loot_compiled["binary_sha256"]:
        raise ValueError("AutoLoot bridge binary changed during registration")
    shutil.copyfile(binary, destination)
    if sha256_file(destination) != compiled["binary_sha256"]:
        raise ValueError("copied DLL is not the compiled artifact")
    write_json("runtime/current.json", manifest)
    write_json("runtime/module_registry.json", owners)
    write_json("AI_INDEX.json", idx)
    errors = validate(manifest, owners)
    if errors:
        raise ValueError("hook/module ownership conflict: " + "; ".join(errors))
    print("ESP_STAGE: real Windows PE32 x86 DLL sha256=",
          compiled["binary_sha256"])
    print("ESP_STAGE: clean 112 GDI/12340 native W2S+DDC; no Lua/D3D ESP hooks; gameplay unverified")

if __name__ == "__main__":
    sys.exit(main())
