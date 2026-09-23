"""Never publish a runnable game artifact from only a supplied EXE."""
from __future__ import annotations
import json
import os
import sys
from pathlib import Path
from manifest_common import ROOT, load_json

def assess(runtime: dict, audit: dict) -> tuple[bool, str]:
    files = runtime.get("files", [])
    if runtime.get("state") == "empty" and not files:
        return False, "EXE_ONLY: Wow.exe is audited but no active x86 DLL or client runtime is registered."
    if runtime.get("state") == "empty" or not isinstance(files, list):
        raise ValueError("inconsistent runtime state")
    exes = [f for f in files if f.get("kind") == "exe"]
    dlls = [f for f in files if f.get("kind") == "dll"]
    if len(exes) != 1 or not dlls:
        raise ValueError("runtime must register exactly one EXE and at least one real DLL")
    if Path(exes[0].get("path", "")).name.lower() != "wow.exe":
        raise ValueError("active EXE must reference the audited Wow.exe")
    target = load_json(ROOT / "runtime/client_exe_target.json")
    if (exes[0].get("path") != target.get("path") or
        exes[0].get("sha256") != target.get("sha256") or
        audit.get("sha256") != target.get("sha256") or
        audit.get("size") != target.get("size")):
        raise ValueError("client differs from selected exact Wow.exe")
    if audit.get("sha256") != exes[0].get("sha256"):
        raise ValueError("registered EXE checksum does not match audited client")
    if not audit.get("version_resource_matches_12340"):
        raise ValueError("exact 3.3.5.12340 version resource is not verified")
    return True, "ACTIVE_RUNTIME: registered client and DLL set; game build and package gates required."

def main() -> int:
    try:
        runtime = load_json(ROOT / "runtime/current.json")
        audit = json.loads((ROOT / "dist/client_exe_audit.json").read_text(encoding="utf-8"))
        ready, reason = assess(runtime, audit)
        report = {"candidate_ready": ready, "reason": reason, "client_sha256": audit["sha256"]}
        Path(ROOT / "dist/candidate_readiness.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as out:
                out.write("ready=" + ("true" if ready else "false") + "\n")
        print("CANDIDATE_READINESS:", "READY" if ready else "EXE_ONLY", reason)
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print("CANDIDATE_READINESS: FAIL", exc)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
