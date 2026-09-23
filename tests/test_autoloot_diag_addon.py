"""The diagnostic addon is not part of the runtime and cannot turn into a game package."""
import hashlib
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from package_autoloot_diag import build, FILES, SOURCE


class AutoLootDiagnosticAddonTests(unittest.TestCase):
    def test_diagnostic_is_disabled_by_default_and_cannot_scan_corpses(self):
        script = (SOURCE / FILES[0]).read_text(encoding="utf-8")
        toc = (SOURCE / FILES[1]).read_text(encoding="utf-8")
        self.assertIn("local enabled = false", script)
        self.assertIn('f:RegisterEvent("LOOT_OPENED")', script)
        self.assertIn('f:RegisterEvent("LOOT_CLOSED")', script)
        self.assertIn('pcall(LootSlot, slot)', script)
        self.assertIn('SLASH_AUTOLOOT335DIAG1 = "/al335"', script)
        self.assertIn("## Interface: 30300", toc)
        self.assertNotIn("SetCVar(", script)

    def test_deterministic_zip_and_fail_closed_provenance(self):
        sha = "1" * 40
        with tempfile.TemporaryDirectory() as tmp:
            a, b = Path(tmp) / "a.zip", Path(tmp) / "b.zip"
            with self.assertRaises(ValueError):
                build(a, "local", "feature/autoloot-12340")
            with self.assertRaises(ValueError):
                build(a, sha, "main")
            build(a, sha, "feature/autoloot-12340")
            build(b, sha, "feature/autoloot-12340")
            self.assertEqual(hashlib.sha256(a.read_bytes()).digest(),
                             hashlib.sha256(b.read_bytes()).digest())
            with zipfile.ZipFile(a) as z:
                meta = json.loads(z.read("diag_info.json"))
                self.assertFalse(meta["native_dll"])
                self.assertFalse(meta["auto_corpse_interaction"])
                self.assertEqual(meta["final_package"], "NOT_RUN")
                self.assertEqual(meta["git_sha"], sha)
                for path in FILES:
                    self.assertEqual(hashlib.sha256(z.read(path)).hexdigest(),
                                     meta["files_sha256"][path])
