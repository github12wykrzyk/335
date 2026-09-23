import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_candidate_package import verify_attestation

class PackageAttestationTests(unittest.TestCase):
    def test_exact_source_binding(self):
        meta = {"schema_version":2,"project":"335","wow_build":12340,"arch":"x86",
                "git_sha":"a"*40,"branch":"work","manifest_sha256":"b"*64,
                "module_registry_sha256":"c"*64}
        verify_attestation(meta,"a"*40,"work","b"*64,"c"*64)
        for values in (("d"*40,"work","b"*64,"c"*64),
                       ("a"*40,"main","b"*64,"c"*64),
                       ("a"*40,"work","d"*64,"c"*64),
                       ("a"*40,"work","b"*64,"d"*64)):
            with self.assertRaises(ValueError):
                verify_attestation(meta,*values)

if __name__ == "__main__":
    unittest.main()
