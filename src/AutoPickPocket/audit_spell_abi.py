"""Read-only analysis of native Pick Pocket cast ABI on the exact pinned 12340 PE."""
from __future__ import annotations
import hashlib, json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    data=(ROOT/"Wow.exe").read_bytes()
    meta=json.loads((ROOT/"runtime/client_exe_target.json").read_text(encoding="utf-8"))
    sha=hashlib.sha256(data).hexdigest()
    if sha!=meta["sha256"] or len(data)!=meta["size"]:
        raise SystemExit("PP_NATIVE_AUDIT: FAIL wrong exact game client")
    pe=struct.unpack_from("<I",data,0x3c)[0]
    assert data[:2]==b"MZ" and data[pe:pe+4]==b"PE\0\0"
    assert struct.unpack_from("<H",data,pe+4)[0]==0x14c
    opt=pe+24
    assert struct.unpack_from("<H",data,opt)[0]==0x10b
    base=struct.unpack_from("<I",data,opt+28)[0]
    section_count=struct.unpack_from("<H",data,pe+6)[0]
    headers=opt+struct.unpack_from("<H",data,pe+20)[0]
    sections=[]
    for i in range(section_count):
        at=headers+40*i
        name=data[at:at+8].split(b"\0")[0].decode("ascii","replace")
        vs,va,sz,raw=struct.unpack_from("<IIII",data,at+8)
        flags=struct.unpack_from("<I",data,at+36)[0]
        sections.append((name,base+va,vs,sz,raw,bool(flags&0x20000000)))
    def locate(va):
        for name,start,vs,sz,raw,x in sections:
            if start<=va<start+min(vs,sz) and x:
                return raw+va-start,name
        raise ValueError(f"candidate 0x{va:08X} not mapped in executable section")
    print(f"PP_CLIENT: pinned sha256={sha} bytes={len(data)} base=0x{base:08X}")
    # Candidate NPC class/reaction entrypoints collected from independent
    # public build-12340 references. Only the exact binary can verify bytes;
    # even matching prologues/xrefs do NOT establish a callable signature.
    candidates = (
        ("historical_CastSpell_GUID", 0x0080DA40),
        ("historical_GetSpellIdByName", 0x00540200),
        ("npc_GetCreatureType_candidate_A", 0x00605570),
        ("npc_GetReaction_candidate_A", 0x006061E0),
        ("npc_GetCreatureType_candidate_B", 0x0071F6E0),
    )
    for name,va in candidates:
        try:
            at,section=locate(va)
        except ValueError as exc:
            print(f"PP_SYMBOL_UNMAPPED: {name} va=0x{va:08X}: {exc}")
            continue
        blob=data[at:at+192]
        print(f"PP_SYMBOL: {name} va=0x{va:08X} section={section} first_192_bytes={blob.hex()}")
        candidates=[(i,blob[i+1]) for i in range(len(blob)-2) if blob[i]==0xc2 and blob[i+2]==0x00]
        print(f"PP_RET_IMM_CANDIDATES: {name} "+repr(candidates[:32]))
        # Only direct E8 CALL xrefs: evidence, not signature proof.
        calls=[]
        for sec_name,start,vs,sz,raw,executable in sections:
            if not executable: continue
            chunk=data[raw:raw+sz]
            for off in range(len(chunk)-4):
                if chunk[off]!=0xe8: continue
                rel=struct.unpack_from("<i",chunk,off+1)[0]
                if start+off+5+rel==va:
                    calls.append((start+off,chunk[max(off-24,0):min(off+16,len(chunk))].hex()))
        print(f"PP_DIRECT_CALLS: {name} count={len(calls)}")
        for call_va,hexdump in calls[:24]:
            print(f"PP_XREF: {name} call_va=0x{call_va:08X} surrounding_bytes={hexdump}")
    print("PP_NATIVE_AUDIT: STATIC_CANDIDATES_ONLY; creature type/reaction ABI and call signatures UNVERIFIED; NEVER invoke from this report alone")
if __name__=="__main__":
    main()
