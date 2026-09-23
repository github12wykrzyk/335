"""STABLE: require recoverable exact accepted binaries; never recompile."""
from __future__ import annotations
import subprocess
import sys
from pathlib import Path
from manifest_common import ROOT

def main():
    verifier = subprocess.run([sys.executable,
        str(ROOT / "tools/verify_exact_runtime_artifacts.py")], cwd=ROOT)
    if verifier.returncode:
        return verifier.returncode
    return subprocess.run([sys.executable, str(ROOT / "tools/package_candidate.py")]
        + sys.argv[1:], cwd=ROOT).returncode

if __name__ == "__main__":
    sys.exit(main())
