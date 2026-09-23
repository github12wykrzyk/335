"""Loader must fail closed on an incomplete or non-PE32 x86 DLL list."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class LoaderPreflightTests(unittest.TestCase):
    def test_all_modules_prechecked_before_first_load(self):
        source = (ROOT / "src/Loader12340/Wow335Loader.c").read_text(encoding="utf-8")
        self.assertIn("static int preflight_module", source)
        self.assertIn("GetFileSizeEx(file, &size)", source)
        self.assertIn("IMAGE_FILE_MACHINE_I386", source)
        self.assertIn("IMAGE_NT_OPTIONAL_HDR32_MAGIC", source)
        self.assertIn("FILE_ATTRIBUTE_REPARSE_POINT", source)
        self.assertIn('if (!preflight_module(names[i])) return 1;', source)
        self.assertLess(source.index("if (!preflight_module(names[i])) return 1;"),
                        source.index("loaded = LoadLibraryExW(path"))
        self.assertIn('L"PRECHECK_PE32_X86_FAILED"', source)
        self.assertIn('L"PRECHECK_PATH"', source)


if __name__ == "__main__":
    unittest.main()
