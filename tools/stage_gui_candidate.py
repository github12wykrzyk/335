"""Native stage for feature/shared-gui-12340; no claims of gameplay success."""
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

BRANCH="feature/shared-gui-12340"
GUI_SRC=["src/SharedGUI/w335_gui_api.h","src/SharedGUI/w335_gui_win32.c"]
def gui_contract():
    return {
        "component":"SharedGUI","sources":GUI_SRC,"requires":[],
        "resources":[
            {"id":"logical:input","mode":"exclusive"},
            {"id":"win32:gui-hotkey-insert","mode":"exclusive"},
            {"id":"win32:gui-popup-window","mode":"exclusive"},
            {"id":"win32:WH_GETMESSAGE","mode":"chain","arbitrator":"Loader"},
            {"id":"win32:WH_CALLWNDPROC","mode":"chain","arbitrator":"Loader"}
        ],
        "build":{"toolchain":"msvc_x86",
                 "sources":["src/SharedGUI/w335_gui_win32.c"],
                 "include_dirs":["src/SharedGUI"],
                 "libraries":["User32.lib","Gdi32.lib","Kernel32.lib"],
                 "cflags":["/TC","/Brepro"],"ldflags":[]}
    }

def prepare_registration(runtime, registry, index, hashes):
    runtime,registry,index=[copy.deepcopy(x) for x in (runtime,registry,index)]
    active=[f["component"] for f in runtime["files"]]
    owners=[m["component"] for m in registry["modules"]]
    indexed=[m["component"] for m in index["modules"]]
    if active not in (["Client12340","AutoLoot","PlayerESP"],
                      ["Client12340","AutoLoot","SharedGUI","PlayerESP"]):
        raise ValueError("unexpected active runtime; refuse overwrite")
    if owners != active[1:] or indexed != active[1:]:
        raise ValueError("registry/index differ from active runtime; refuse overwrite")
    if runtime["target"].get("build")!=12340:raise ValueError("wrong game build")
    for comp in ("AutoLoot","SharedGUI","PlayerESP"):
        value=hashes.get(comp)
        if not isinstance(value,str) or len(value)!=64 or any(
            c not in "0123456789abcdef" for c in value):
            raise ValueError("no real PE32 x86 SHA256 for "+comp)
    runtime["files"][1]["sha256"]=hashes["AutoLoot"]
    if "SharedGUI" not in active:
        runtime["files"].insert(2,{
        "component":"SharedGUI","path":"runtime/WoW335GUI.dll",
        "version":"0.1.0-shared-gui-test",
        "sha256":hashes["SharedGUI"],"arch":"x86",
        "canonical_source":"src/SharedGUI/w335_gui_win32.c",
        "depends_on":[],"kind":"dll"})
    runtime["files"][2]["sha256"]=hashes["SharedGUI"]
    runtime["files"][2]["version"]="0.1.1-managed-dll-list-test"
    runtime["files"][3]["sha256"]=hashes["PlayerESP"]
    runtime["files"][3]["version"]="0.6.0-shared-gui-test"
    runtime["files"][3]["depends_on"]=["SharedGUI"]
    runtime["release_id"]="feature-shared-gui-12340-test"
    runtime["compatibility_sets"]=[{
        "id":"client12340-autoloot-sharedgui-playeresp-test",
        "components":["Client12340","AutoLoot","SharedGUI","PlayerESP"]}]
    esp=registry["modules"][-1]
    esp["requires"]=["SharedGUI"]
    if GUI_SRC[0] not in esp["sources"]:esp["sources"].append(GUI_SRC[0])
    if "src/SharedGUI" not in esp["build"]["include_dirs"]:
        esp["build"]["include_dirs"].append("src/SharedGUI")
    if "SharedGUI" not in owners:
        registry["modules"].insert(1,gui_contract())
        index["modules"].insert(1,{
            "component":"SharedGUI",
            "source":"src/SharedGUI/w335_gui_win32.c",
            "docs":"src/SharedGUI/README.md"})
    errors=validate(runtime,registry)
    if errors:raise ValueError("resource/dependency conflict: "+"; ".join(errors))
    return runtime,registry,index

def write_json(path,obj):
    (ROOT/path).write_text(
        json.dumps(obj,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--stage",action="store_true",required=True)
    ap.parse_args()
    if os.name!="nt" or os.getenv("GITHUB_REF_NAME")!=BRANCH:
        raise ValueError("only Windows feature/shared-gui-12340 may stage")
    runtime=load_json(ROOT/"runtime/current.json")
    registry=load_json(ROOT/"runtime/module_registry.json")
    index=load_json(ROOT/"AI_INDEX.json")
    active=[m["component"] for m in registry["modules"]]
    if active not in (["AutoLoot","PlayerESP"],["AutoLoot","SharedGUI","PlayerESP"]):
        raise ValueError("unexpected native baseline")
    for entry in runtime["files"]:
        if entry["kind"]=="dll" and sha256_file(ROOT/entry["path"])!=entry["sha256"]:
            raise ValueError(entry["component"]+": registered DLL SHA mismatch")
    if "SharedGUI" not in active and (ROOT/"runtime/WoW335GUI.dll").exists():
        raise ValueError("unexpected unregistered GUI binary")
    vcvars=find_vcvars()
    folder=ROOT/"dist/gui-stage"
    descriptors=[copy.deepcopy(registry["modules"][0]),
                 gui_contract(),copy.deepcopy(registry["modules"][-1])]
    descriptors[2]["requires"]=["SharedGUI"]
    if GUI_SRC[0] not in descriptors[2]["sources"]:
        descriptors[2]["sources"].append(GUI_SRC[0])
    if "src/SharedGUI" not in descriptors[2]["build"]["include_dirs"]:
        descriptors[2]["build"]["include_dirs"].append("src/SharedGUI")
    filenames={"AutoLoot":"runtime/AutoLoot335.dll",
               "SharedGUI":"runtime/WoW335GUI.dll",
               "PlayerESP":"runtime/PlayerESP335.dll"}
    hashes={}
    for descriptor in descriptors:
        comp=descriptor["component"]
        result=compile_module(descriptor,{"path":filenames[comp]},vcvars,
                              folder,verify_registered=False)
        built=folder/comp/(ROOT/filenames[comp]).name
        if sha256_file(built)!=result["binary_sha256"]:
            raise ValueError(comp+": compiled SHA differs")
        hashes[comp]=result["binary_sha256"]
    rt,rg,idx=prepare_registration(runtime,registry,index,hashes)
    for comp,path in filenames.items():
        target=ROOT/path
        shutil.copyfile(folder/comp/target.name,target)
        if sha256_file(target)!=hashes[comp]:
            raise ValueError(comp+": copy SHA differs")
    write_json("runtime/current.json",rt)
    write_json("runtime/module_registry.json",rg)
    write_json("AI_INDEX.json",idx)
    print("SHARED_GUI_STAGE: PE32 x86 compiled and registered",hashes)
    print("SHARED_GUI_STAGE: gameplay unverified")

if __name__=="__main__":sys.exit(main())
