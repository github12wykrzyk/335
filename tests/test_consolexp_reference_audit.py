"""Test ZIP controls without trusting either uploaded artifact as executable."""
import io
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from audit_consolexp_reference import git_blob_sha1, inspect_archive, safe_member


def make_zip(entries):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        for name, content in entries:
            z.writestr(name, content)
    return buf.getvalue()


class ConsoleXPReferenceAuditTests(unittest.TestCase):
    def inspect(self, payload, role="source"):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "source.zip"
            path.write_bytes(payload)
            # Real path display in inspect_archive is relative to ROOT.
            # Invalid cases must fail before reaching the display stage.
            return inspect_archive(path, git_blob_sha1(payload), role)

    def test_git_blob_hash(self):
        self.assertEqual(git_blob_sha1(b"test"), "30d74d258442c7c65512eafab474568dd706c430")

    def test_rejects_traversal(self):
        self.assertFalse(safe_member("../escape.cpp"))
        self.assertFalse(safe_member("/abs/source.cpp"))
        self.assertFalse(safe_member("C:/file.cpp"))
        with self.assertRaisesRegex(ValueError, "unsafe ZIP member"):
            self.inspect(make_zip([("../escape.cpp", "x")]))

    def test_rejects_duplicate_case_insensitive(self):
        with self.assertRaisesRegex(ValueError, "duplicate ZIP member"):
            self.inspect(make_zip([("Game.cpp", "x"), ("game.cpp", "y")]))

    def test_requires_native_sources_in_source_archive(self):
        with self.assertRaisesRegex(ValueError, "no native source"):
            self.inspect(make_zip([("README.md", "readme")]))

    def test_rejects_identity_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "source.zip"
            path.write_bytes(make_zip([("Game.cpp", "x")]))
            with self.assertRaisesRegex(ValueError, "blob changed"):
                inspect_archive(path, "0" * 40, "source")


if __name__ == "__main__":
    unittest.main()
