import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_exact_runtime_artifacts import verify_exact_artifacts

class ExactRuntimeTests(unittest.TestCase):
    def test_empty_runtime_not_releasable(self):
        self.assertTrue(verify_exact_artifacts({"state":"empty","files":[]}))
    def test_no_cache_fails_closed(self):
        rt = {"state":"candidate","files":[
            {"kind":"exe","component":"Game","path":"Wow.exe","sha256":"0"*64},
            {"kind":"dll","component":"Core","path":"src/README.md","sha256":"0"*64},
        ]}
        self.assertTrue(verify_exact_artifacts(rt))

if __name__ == "__main__":
    unittest.main()
