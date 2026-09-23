from __future__ import annotations
import argparse,hashlib,json,os,subprocess,sys,zipfile
from pathlib import Path
from manifest_common import ROOT,load_json,repo_path,sha256_file
def main():
    ap=argparse.ArgumentParser();ap.add_argument("--package",required=True);ap.add_argument("--metadata",required=True);a=ap.parse_args()
    for v in ("verify_repo.py","verify_current.py"):
        if subprocess.call([sys.executable,str(ROOT/"tools"/v)],cwd=ROOT): return 1
    r=load_json(ROOT/"runtime/current.json")
    files=r["files"]; exes=[x for x in files if x.get("kind")=="exe"]; dlls=[x for x in files if x.get("kind")=="dll"]
    if r.get("state")=="empty" or len(exes)!=1 or not dlls:
        print("NO_RUNTIME: a verified 12340 x86 EXE and active DLL set are required; game package blocked"); return 2
    if any(x.get("kind") not in ("exe","dll") for x in files):
        print("Unsupported runtime data entry: fail closed");return 3
    order=[x["component"] for x in files]; seen=set()
    for x in files:
        name=Path(x["path"]).name
        if name.casefold() in seen or name in ("dlls.txt","candidate_metadata.json"):
            print("Duplicate/forbidden root filename",name);return 3
        seen.add(name.casefold())
        if x.get("depends_on") and any(order.index(d)>order.index(x["component"]) for d in x["depends_on"]):
            print("Dependency load order violation",name);return 3
    output=repo_path(a.package); output.parent.mkdir(parents=True,exist_ok=True)
    names=[Path(x["path"]).name for x in files]
    dllorder=[Path(x["path"]).name for x in dlls]
    content="\r\n".join(dllorder)+"\r\n"
    with zipfile.ZipFile(output,"w",zipfile.ZIP_DEFLATED) as z:
        for x in files:
            p=repo_path(x["path"]); zi=zipfile.ZipInfo(p.name,(1980,1,1,0,0,0));zi.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(zi,p.read_bytes())
        zi=zipfile.ZipInfo("dlls.txt",(1980,1,1,0,0,0));z.writestr(zi,content.encode("ascii"))
    meta={"schema_version":1,"project":"335","wow_build":12340,"arch":"x86","branch":os.getenv("GITHUB_REF_NAME","local"),"git_sha":os.getenv("GITHUB_SHA","local-uncommitted"),"package_name":output.name,"package_sha256":sha256_file(output),"package_size":output.stat().st_size,"exe":Path(exes[0]["path"]).name,"dlls":dllorder,"files":[{"name":Path(x["path"]).name,"sha256":x["sha256"],"kind":x["kind"]} for x in files]}
    p=repo_path(a.metadata);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(meta,indent=2)+"\n",encoding="utf-8")
    print("CANDIDATE PACKAGE:",output,"sha256:",meta["package_sha256"]);return 0
if __name__=="__main__":raise SystemExit(main())
