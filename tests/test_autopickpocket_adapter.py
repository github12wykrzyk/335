"""Portable adapter simulation; actual Windows PE32 x86 build runs in Actions."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class NativeAdapterTests(unittest.TestCase):
    def test_mock_object_manager_guid_cast_and_history(self):
        cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not cc:
            if os.name=="nt": self.skipTest("MSVC x86 adapter test runs in candidate workflow")
            self.fail("C99 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="pp335-adapter-") as tmp:
            exe=Path(tmp)/("pp_adapter.exe" if os.name=="nt" else "pp_adapter")
            cmd=[cc,"-std=c99","-O2","-Wall","-Wextra","-Werror","-pedantic",
                 str(ROOT/"src/AutoPickPocket/autopickpocket_core.c"),
                 str(ROOT/"src/AutoPickPocket/autopickpocket_12340_adapter.c"),
                 str(ROOT/"tests/autopickpocket_12340_adapter_test.c"),
                 "-o",str(exe)]
            c=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(c.returncode,0,c.stdout+c.stderr)
            run=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("PP12340 native adapter mock: PASS",run.stdout)
if __name__=="__main__":
    unittest.main()
