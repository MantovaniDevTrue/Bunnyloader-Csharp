import base64
import hashlib
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

spec = importlib.util.spec_from_file_location("stage_refs", Path(__file__).parents[1] / "tools/stage_compiler_references.py")
stage_refs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage_refs)

class ReferenceStageTests(unittest.TestCase):
    def test_digest_manifest_paths_and_tamper_rejection(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            package = root / "refs.nupkg"
            with zipfile.ZipFile(package, "w") as archive:
                for name in stage_refs.NAMES:
                    archive.writestr("ref/net8.0/" + name + ".dll", b"MZfixture:" + name.encode())
            metadata = root / "catalog.json"
            metadata.write_text(json.dumps({"packageHashAlgorithm": "SHA512",
                "packageHash": base64.b64encode(hashlib.sha512(package.read_bytes()).digest()).decode()}))
            sdk = root / "Sdk.dll"
            sdk.write_bytes(b"MZsdk-fixture")
            output = root / "staged"
            stage_refs.stage(package, sdk, output, metadata)
            self.assertEqual(len(list(output.glob("*.dll"))), 13)
            self.assertEqual((output / "BunnySourceSdk.dll").read_bytes(), sdk.read_bytes())
            package.write_bytes(package.read_bytes() + b"changed")
            with self.assertRaisesRegex(ValueError, "ReferencePackSHA512Mismatch"):
                stage_refs.stage(package, sdk, root / "rejected", metadata)
            self.assertFalse((root / "rejected").exists())

if __name__ == "__main__":
    unittest.main()
