import subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class FailClosedPackageTests(unittest.TestCase):
    def test_no_runtime_cannot_be_published(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as d:
            a=Path(d)
            p=subprocess.run([sys.executable,str(ROOT/"tools/package_candidate.py"),"--package",str(a/"candidate.zip"),"--metadata",str(a/"candidate_metadata.json")],cwd=ROOT,capture_output=True,text=True)
            self.assertNotEqual(p.returncode,0,p.stdout+p.stderr)
            self.assertFalse((a/"candidate.zip").exists())
if __name__=="__main__": unittest.main()
