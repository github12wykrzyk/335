"""Verify 3.3.5 packet framing offline, without contacting the game server."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class PacketCodec(unittest.TestCase):
    def test_client_datastore_layout_and_bounds(self):
        cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not cc:
            if os.name=="nt":self.skipTest("native MSVC packet-codec test runs in x86 build")
            self.fail("C99 compiler is required")
        with tempfile.TemporaryDirectory(prefix="pp335-packet-") as t:
            out=Path(t)/("packet.exe" if os.name=="nt" else "packet")
            cmd=[cc,"-std=c99","-O2","-Wall","-Wextra","-Werror","-pedantic",
                 str(ROOT/"src/AutoPickPocket/autopickpocket_packet_12340.c"),
                 str(ROOT/"tests/autopickpocket_packet_test.c"),"-o",str(out)]
            c=subprocess.run(cmd,cwd=ROOT,text=True,capture_output=True)
            self.assertEqual(c.returncode,0,c.stdout+c.stderr)
            x=subprocess.run([str(out)],cwd=ROOT,text=True,capture_output=True)
            self.assertEqual(x.returncode,0,x.stdout+x.stderr)
            self.assertIn("PP335 packet codec: PASS",x.stdout)
if __name__=="__main__":unittest.main()
