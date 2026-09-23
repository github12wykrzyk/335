"""Native PP runtime registration and exact-binary refresh on its isolated feature branch only.

Runs on Windows after native code/audits exist. Builds the exact PE32 x86 DLL
with the SAME pinned recipe/toolchain used by build_active.py. Does not publish
a game package or modify work/main. The workflow verifies a second full native
rebuild and commits exact bytes + manifest in one feature-only atomic change.
"""
from __future__ import annotations
import hashlib
import json
import os
import shutil
import sys
from pathlib import Path
from build_active import compile_module, find_vcvars, inspect_dll
from manifest_common import ROOT
from verify_module_registry import validate

PP="AutoPickPocket"
NAME="AutoPickPocket335.dll"
SOURCES=[
 "src/AutoPickPocket/autopickpocket_core.c",
 "src/AutoPickPocket/autopickpocket_core.h",
 "src/AutoPickPocket/autopickpocket_12340_adapter.c",
 "src/AutoPickPocket/autopickpocket_12340_adapter.h",
 "src/AutoPickPocket/autopickpocket_win32_host.c",
 "src/AutoPickPocket/autopickpocket_win32_host.h",
 "src/AutoPickPocket/autopickpocket_game_policies.c",
]
COMPILE=[p for p in SOURCES if p.endswith(".c")]
HOOKS=("win32:WH_GETMESSAGE","win32:WH_CALLWNDPROC")

def dump(path, data):
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")

def main():
    if os.name != "nt" or os.environ.get("GITHUB_REF_NAME")!="feature/autopickpocket-12340":
        raise RuntimeError("PP registration is Windows-only and isolated to the PP feature branch")
    manifest_path=ROOT/"runtime/current.json"
    registry_path=ROOT/"runtime/module_registry.json"
    index_path=ROOT/"AI_INDEX.json"
    runtime=json.loads(manifest_path.read_text(encoding="utf-8"))
    registry=json.loads(registry_path.read_text(encoding="utf-8"))
    index=json.loads(index_path.read_text(encoding="utf-8"))
    # A source fix must refresh the already registered DLL and its SHA.
    # Never publish the previous binary with newly changed source files.
    names=[x["component"] for x in runtime["files"]]
    if PP in names:
        if (names!=["Client12340","AutoLoot",PP] or
            [m["component"] for m in registry["modules"]]!=["AutoLoot",PP] or
            [m["component"] for m in index["modules"]]!=["AutoLoot",PP]):
            raise RuntimeError("inconsistent registered PP ownership")
        module=registry["modules"][-1]
        if module["sources"]!=SOURCES or module["build"]["sources"]!=COMPILE:
            raise RuntimeError("registered PP build recipe differs from current source")
        row=runtime["files"][-1]
        if row["path"]!="runtime/"+NAME or row["kind"]!="dll" or row["arch"]!="x86":
            raise RuntimeError("unexpected existing PP runtime entry")
        report=compile_module(module,row,find_vcvars(),ROOT/"dist/native",
                              verify_registered=False)
        compiled=ROOT/"dist/native"/PP/NAME
        inspect_dll(compiled)
        digest=hashlib.sha256(compiled.read_bytes()).hexdigest()
        if digest!=report["binary_sha256"]:
            raise RuntimeError("native PP output changed after build")
        target=ROOT/"runtime"/NAME
        shutil.copyfile(compiled,target)
        if hashlib.sha256(target.read_bytes()).hexdigest()!=digest:
            raise RuntimeError("registered DLL changed during copy")
        row["version"]="1.0.6-test+sha."+digest[:12]
        row["sha256"]=digest
        errors=validate(runtime,registry)
        if errors:
            raise RuntimeError("invalid refreshed PP registry: "+"; ".join(errors))
        dump(manifest_path,runtime)
        print("PP_NATIVE_PE32_X86_REFRESHED:",digest,
              "; not verified in game")
        return 0
    if [x["component"] for x in runtime["files"]] != ["Client12340","AutoLoot"]:
        raise RuntimeError("PP registration requires the canonical work AutoLoot-only runtime")
    if [m["component"] for m in registry["modules"]] != ["AutoLoot"]:
        raise RuntimeError("unexpected module registry; refusing implicit replacement")
    if [m["component"] for m in index["modules"]] != ["AutoLoot"]:
        raise RuntimeError("unexpected AI_INDEX; refusing implicit replacement")
    # Both module hooks use the OS CallNextHookEx chain. Neither installs an
    # additional independent launcher or replaces an existing game detour.
    autoloot=registry["modules"][0]
    for resource in autoloot["resources"]:
        if resource["id"] in HOOKS:
            if resource["mode"] != "exclusive":
                raise RuntimeError("unrecognized pre-existing hook ownership")
            resource["mode"]="chain"
            resource["arbitrator"]="AutoLoot"
    pp={
      "component":PP,
      "sources":SOURCES,
      "requires":[],
      "resources":[
        {"id":"wow12340:object-manager","mode":"observe"},
        {"id":"wow12340:0x00819210-framescript-execute","mode":"observe"},
        {"id":"wow12340:0x0080DA40-guid-spell-cast","mode":"exclusive"},
        {"id":"wow12340:0x0071F300-native-creature-type","mode":"observe"},
        {"id":"logical:targeting","mode":"observe"},
        {"id":"logical:loot-ui","mode":"observe"},
        {"id":"logical:spell-cast","mode":"exclusive"},
        {"id":"win32:WH_GETMESSAGE","mode":"chain","arbitrator":"AutoLoot"},
        {"id":"win32:WH_CALLWNDPROC","mode":"chain","arbitrator":"AutoLoot"}
      ],
      "build":{
        "toolchain":"msvc_x86",
        "sources":COMPILE,
        "include_dirs":["src/AutoPickPocket"],
        "libraries":["Advapi32.lib","User32.lib"],
        "cflags":["/TC","/Brepro","/WX"],
        "ldflags":[]
      }
    }
    # Compiled in dist/native/AutoPickPocket, identical to full active builder.
    file_row={"path":"runtime/"+NAME,"sha256":"0"*64}
    result=compile_module(pp,file_row,find_vcvars(),ROOT/"dist/native",verify_registered=False)
    binary=ROOT/"dist/native"/PP/NAME
    inspect_dll(binary)
    digest=hashlib.sha256(binary.read_bytes()).hexdigest()
    if digest!=result["binary_sha256"]:
        raise RuntimeError("native binary changed after compile")
    dest=ROOT/"runtime"/NAME
    shutil.copyfile(binary,dest)
    if hashlib.sha256(dest.read_bytes()).hexdigest()!=digest:
        raise RuntimeError("registered PP DLL differs from native Windows build")
    runtime["files"].append({
      "component":PP,
      "path":"runtime/"+NAME,
      "version":"1.0.0-test+sha."+digest[:12],
      "sha256":digest,
      "arch":"x86",
      "canonical_source":"src/AutoPickPocket/autopickpocket_win32_host.c",
      "depends_on":[],
      "kind":"dll"
    })
    runtime["release_id"]="feature-autopickpocket-universal-12340"
    runtime["compatibility_sets"]=[{
      "id":"client12340-autoloot-autopickpocket",
      "components":["Client12340","AutoLoot",PP]
    }]
    registry["modules"].append(pp)
    index["modules"].append({
      "component":PP,"source":"src/AutoPickPocket/autopickpocket_win32_host.c",
      "docs":"src/AutoPickPocket/README.md"
    })
    problems=validate(runtime,registry)
    if problems:
        raise RuntimeError("Invalid combined resource/dependency contract: "+"; ".join(problems))
    dump(manifest_path,runtime)
    dump(registry_path,registry)
    dump(index_path,index)
    print("PP_NATIVE_PE32_X86_REGISTERED:",digest,"; source-only build != in-game verification")
    return 0

if __name__=="__main__":
    try: raise SystemExit(main())
    except Exception as exc:
        print("PP_RUNTIME_REGISTRATION: FAIL",exc)
        raise SystemExit(1)
