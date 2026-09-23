"""Launcher and updater bind the complete managed DLL set to exact local SHA256."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ModuleLockTests(unittest.TestCase):
    def test_native_loader_rejects_missing_or_mismatched_lock_before_any_load(self):
        source = (ROOT / "src/Loader12340/Wow335Loader.c").read_text(encoding="utf-8")
        self.assertIn('L".wow335_updater', source)
        self.assertIn('modules.lock"', source)
        self.assertIn('L"MODULE_LOCK_SHA256_FAILED"', source)
        self.assertIn("CALG_SHA_256", source)
        self.assertIn("CryptGetHashParam(", source)
        self.assertIn('strcmp(line, list_digest) != 0', source)
        self.assertIn('strcmp(module_digest, separator) != 0', source)
        self.assertIn("if (!preflight_module_lock(names, count)) return 1;", source)
        self.assertLess(source.index("if (!preflight_module_lock(names, count)) return 1;"),
                        source.index("loaded = LoadLibraryExW(path"))
        self.assertIn('L"PRECHECK_PE32_X86_FAILED"', source)

    def test_managed_updater_seals_module_order_and_rolls_back_lock(self):
        source = (ROOT / "tools/updater/UpdaterEpochLoaderFeature.cs").read_text(encoding="utf-8")
        self.assertIn('EpochModuleLockPath(string root)', source)
        self.assertIn('EpochModuleLockContent(string root', source)
        self.assertIn('EpochSyncModuleLock(root);', source)
        self.assertIn('state["module_lock_sha256"] = digest;', source)
        self.assertIn("Encoding.ASCII.GetBytes(", source)
        self.assertIn('EpochCheckFile(moduleLock, lockDigest)', source)
        self.assertIn('if (lockDigest.Length != 0) File.Delete(moduleLock);', source)
        self.assertIn('EpochCheckFile(Path.Combine(root, name), digest);', source)


if __name__ == "__main__":
    unittest.main()
