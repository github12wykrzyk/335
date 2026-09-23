from __future__ import annotations
import argparse,hashlib,json,re,struct,zipfile
from pathlib import Path
from manifest_common import repo_path,sha256_file
def pe32(data):
    if len(data)<0x40 or data[:2]!=b"MZ":return False
    off=struct.unpack_from("<I",data,0x3c)[0]
    return off+26<=len(data) and data[off:off+4]==b"PE\0\0" and struct.unpack_from("<H",data,off+4)[0]==0x14c and struct.unpack_from("<H",data,off+24)[0]==0x10b
def main():
    ap=argparse.ArgumentParser();ap.add_argument("--package",required=True);ap.add_argument("--metadata",required=True);ap.add_argument("--finalize",action="store_true");a=ap.parse_args()
    try:
        p=repo_path(a.package); m=json.loads(repo_path(a.metadata).read_text(encoding="utf-8"))
        assert m["project"]=="335" and m["wow_build"]==12340 and m["arch"]=="x86"
        assert re.fullmatch("[0-9a-f]{64}",m["package_sha256"])
        assert m["package_name"]==p.name and m["package_size"]==p.stat().st_size and sha256_file(p)==m["package_sha256"]
        with zipfile.ZipFile(p) as z:
            names=z.namelist();assert len(names)==len(set(n.casefold() for n in names))
            assert all("/" not in n and "\\" not in n and n not in (".","..") for n in names)
            assert names==[x["name"] for x in m["files"]]+["dlls.txt"]
            exes=[x for x in m["files"] if x["kind"]=="exe"]; dlls=[x for x in m["files"] if x["kind"]=="dll"]
            assert len(exes)==1 and dlls and m["exe"]==exes[0]["name"] and m["dlls"]==[x["name"] for x in dlls]
            assert z.read("dlls.txt")==("\r\n".join(m["dlls"])+"\r\n").encode("ascii")
            for x in m["files"]:
                data=z.read(x["name"])
                assert re.fullmatch("[0-9a-f]{64}",x["sha256"]) and hashlib.sha256(data).hexdigest()==x["sha256"] and pe32(data)
        print("FINAL_PACKAGE: PASS; 12340 x86; git_sha:",m["git_sha"]);return 0
    except Exception as e:
        print("FINAL_PACKAGE: FAIL",str(e));return 1
if __name__=="__main__":raise SystemExit(main())
