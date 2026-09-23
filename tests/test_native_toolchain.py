import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from native_toolchain import PINNED_VC_VERSION, PINNED_WINDOWS_SDK, VCVARS_ARGS


class PinnedNativeToolchainTests(unittest.TestCase):
    def test_all_native_build_paths_share_explicit_toolset_and_sdk(self):
        self.assertEqual(PINNED_VC_VERSION, "14.29")
        self.assertEqual(PINNED_WINDOWS_SDK, "10.0.19041.0")
        self.assertEqual(VCVARS_ARGS,
                         "x86 10.0.19041.0 -vcvars_ver=14.29")
        for path in ("tools/build_active.py", "tools/publish_autoloot_runtime.py"):
            source = (ROOT / path).read_text(encoding="utf-8")
            self.assertIn("VCVARS_ARGS", source)
            self.assertNotIn(' + "\\" x86\\r\\n"', source)

    def test_registered_build_keeps_exact_rebuild_gate(self):
        source = (ROOT / "tools/build_active.py").read_text(encoding="utf-8")
        self.assertIn('if actual != runtime_file["sha256"]:', source)
        self.assertIn('EXACT_PE32_X86_REBUILD_PASS', source)
        self.assertIn('"toolchain": {"vcvars_args": VCVARS_ARGS', source)


if __name__ == "__main__":
    unittest.main()
