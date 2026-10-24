"""Regress the exact missing-entry failure using the production package validator."""
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mods"))
from pack import pack_bytes
from publish import Package, Fail

SAMPLE = ROOT / "samples/FiveLoadersCSharp/Playable"


class PackageImportTests(unittest.TestCase):
    def test_delivered_archive_passes_real_catalog_import_rules(self):
        data = pack_bytes(SAMPLE)
        self.assertEqual(data, pack_bytes(SAMPLE))
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "qa.bl"
            path.write_bytes(data)
            package = Package(path)
            try:
                self.assertEqual(package.version, "0.1.2")
                entry = "content/" + package.manifest["entry"]
                self.assertIn(entry, package.names)
                self.assertTrue(package.zip.read(entry))
            finally:
                package.zip.close()

    def test_both_packager_and_import_validator_reject_missing_entry(self):
        data = pack_bytes(SAMPLE)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "broken.bl"
            with zipfile.ZipFile(io.BytesIO(data)) as source, zipfile.ZipFile(path, "w") as target:
                for name in source.namelist():
                    if name != "content/main.cs":
                        target.writestr(name, source.read(name))
            with self.assertRaisesRegex(Fail, "sem content/main.cs"):
                Package(path)
            copied = Path(temp) / "sources"
            shutil.copytree(SAMPLE, copied)
            (copied / "content/main.cs").unlink()
            with self.assertRaisesRegex(ValueError, "sem content/main.cs"):
                pack_bytes(copied)

    def test_nested_csharp_entry_and_native_and_texture_keep_their_import_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest = json.loads((SAMPLE / "manifest.json").read_text())
            manifest["entry"] = "Common/Entry.cs"
            (root / "manifest.json").write_text(json.dumps(manifest))
            entry = root / "content/Common/Entry.cs"
            entry.parent.mkdir(parents=True)
            entry.write_text("namespace Entry;")
            self.assertTrue(pack_bytes(root))
            manifest["entry"] = "@native"
            (root / "manifest.json").write_text(json.dumps(manifest))
            native = root / "native/arm64-v8a/libmod.so"
            native.parent.mkdir(parents=True)
            native.write_bytes(b"native-fixture")
            self.assertTrue(pack_bytes(root))
            manifest["type"] = "texture"
            manifest.pop("entry")
            (root / "manifest.json").write_text(json.dumps(manifest))
            image = root / "content/Images/Item_1.png"
            image.parent.mkdir(parents=True)
            image.write_bytes(b"png-fixture")
            self.assertTrue(pack_bytes(root))


if __name__ == "__main__":
    unittest.main()
