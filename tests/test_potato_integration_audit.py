import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tools.potato_integration_audit import NEW_DIRS, NEW_FILES, REVIEW_FILES


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/potato_integration_audit.py"


class PotatoIntegrationAuditTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "port"
        self.target = self.root / "original"
        for path in NEW_DIRS:
            (self.source / path).mkdir(parents=True, exist_ok=True)
        for path in NEW_FILES:
            file = self.source / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("new from csharp\n", encoding="utf-8")
        for path in REVIEW_FILES:
            original = self.target / path
            original.parent.mkdir(parents=True, exist_ok=True)
            original.write_text("original content\n", encoding="utf-8")
            source = self.source / path
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text("csharp content\n", encoding="utf-8")

    def run_audit(self, *extra):
        return subprocess.run(
            [
                sys.executable, str(SCRIPT), "--source", str(self.source),
                "--target", str(self.target), *extra,
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    def test_default_is_dry_run(self):
        result = self.run_audit()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ADD_NEW", result.stdout)
        self.assertIn("MERGE_MANUALLY", result.stdout)
        self.assertFalse((self.target / NEW_FILES[0]).exists())
        self.assertFalse((self.target / NEW_DIRS[0]).exists())

    def test_apply_only_new_without_overwriting(self):
        result = self.run_audit("--apply-additions")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.target / NEW_FILES[0]).read_text(), "new from csharp\n")
        self.assertTrue((self.target / NEW_DIRS[0]).is_dir())
        self.assertEqual(
            (self.target / "app/src/main/cpp/CMakeLists.txt").read_text(),
            "original content\n",
        )
        repeat = self.run_audit("--apply-additions")
        self.assertEqual(repeat.returncode, 0, repeat.stderr)
        self.assertIn("ALREADY_IDENTICAL", repeat.stdout)

    def test_collision_aborts_before_any_copy(self):
        path = self.target / NEW_FILES[0]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("upstream different\n", encoding="utf-8")
        result = self.run_audit("--apply-additions")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("COLLISION_REVIEW", result.stdout)
        self.assertEqual(path.read_text(), "upstream different\n")
        self.assertFalse((self.target / NEW_DIRS[0]).exists())


if __name__ == "__main__":
    unittest.main()
