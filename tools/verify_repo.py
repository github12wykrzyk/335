from __future__ import annotations
from manifest_common import ROOT,load_json,repo_path
REQUIRED=["AGENTS.md","AI_START_HERE.md","AI_INDEX.json","CURRENT.json","runtime/current.json","runtime/client_exe_target.json","PROJECT_INSTRUCTIONS.md","runtime/ai_experiments.json","runtime/module_registry.json","tools/verify_module_registry.py","tools/ai_experiments.py",".github/workflows/ai_experiments.yml","tools/verify_repo.py","tools/verify_current.py","tools/build_active.py","tools/verify_exact_runtime_artifacts.py","tools/package_exact_current.py","tools/package_candidate.py","tools/verify_candidate_package.py",".github/workflows/verify.yml",".github/workflows/build_updater.yml",".github/workflows/build_work_candidate.yml",".github/workflows/pre_promote_stable.yml"]
TARGET={"product":"World of Warcraft","version":"3.3.5a","build":12340,"platform":"Windows","architecture":"x86"}
def main():
    err=[]
    for p in REQUIRED:
        if not repo_path(p).is_file(): err.append("missing "+p)
    try:
        idx=load_json(ROOT/"AI_INDEX.json"); cur=load_json(ROOT/"CURRENT.json"); runtime=load_json(ROOT/"runtime/current.json")
    except Exception as e:
        print("REPOSITORY VERIFICATION: FAIL",e); return 1
    if any(x.get("schema_version")!=1 for x in (idx,cur,runtime)): err.append("schema_version must be 1")
    if idx.get("target")!=TARGET or runtime.get("target")!=TARGET: err.append("wrong target")
    if cur.get("project")!="335" or cur.get("runtime_manifest")!="runtime/current.json": err.append("bad current project/manifest")
    if idx.get("read_order")!=REQUIRED[:5]: err.append("wrong mandatory read order")
    if cur.get("canonical_source_root")!="src": err.append("wrong source root")
    selected=load_json(ROOT/"runtime/client_exe_target.json")
    if cur.get("client_exe_target_manifest")!="runtime/client_exe_target.json" or idx.get("canonical",{}).get("client_exe_target")!="runtime/client_exe_target.json": err.append("wrong client routing")
    if selected.get("target_build")!=12340 or selected.get("architecture")!="x86" or selected.get("name")!="Wow.exe" or selected.get("path")!="Wow.exe" or not isinstance(selected.get("size"),int) or selected["size"]<512 or not isinstance(selected.get("sha256"),str) or len(selected["sha256"])!=64: err.append("invalid selected client metadata")
    for m in idx.get("modules",[]):
        for k in ("source","docs"):
            if m.get(k) and not repo_path(m[k]).exists(): err.append("missing module "+m[k])
    from ai_experiments import validate as validate_experiments
    ledger = load_json(ROOT/"runtime/ai_experiments.json")
    err.extend(validate_experiments(ledger))
    if len(idx.get("modules", [])) != len([f for f in runtime.get("files", []) if isinstance(f, dict) and f.get("kind") == "dll"]): err.append("AI_INDEX modules and active DLL count differ")
    if err:
        print("REPOSITORY VERIFICATION: FAIL"); [print(" -",e) for e in err]; return 1
    print("REPOSITORY VERIFICATION: PASS; target 12340 x86; active files",len(runtime.get("files",[]))); return 0
if __name__=="__main__": raise SystemExit(main())
