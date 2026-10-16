"""Negative cases for the public documentation/checkpoint guard."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('repository_checks', Path(__file__).resolve().parents[1] / 'tools/check_repository.py')
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)


class RepositoryChecks(unittest.TestCase):
    def test_local_links_missing_anchor_and_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'target.md').write_text('# Build\n\n## Workflows\n')
            doc = root / 'README.md'
            doc.write_text('[valid](target.md#workflows) [remote](https://example.org/no-network)')
            self.assertEqual(guard.link_errors(doc, root), [])
            doc.write_text('[missing](absent.md) [anchor](target.md#absent) [escape](../private.md)')
            errors = guard.link_errors(doc, root)
            self.assertEqual(len(errors), 3)
            self.assertTrue(any('missing target' in e for e in errors))
            self.assertTrue(any('missing anchor' in e for e in errors))
            self.assertTrue(any('escapes' in e for e in errors))

    def test_code_fences_are_not_navigation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            doc = root / 'README.md'
            doc.write_text('```csharp\n[sample](does-not-exist)\n```\n~~~\n[also](missing)\n~~~')
            self.assertEqual(guard.link_errors(doc, root), [])

    def test_duplicate_unicode_anchors(self):
        self.assertEqual(guard.anchors('# Ação\n## Ação\n## `ref` e `out`'),
                         {'ação', 'ação-1', 'ref-e-out'})

    def test_checkpoint_rejects_version_and_package_corruption(self):
        # Reuse the real deterministic packer with a minimal independent mod fixture.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            base = root / guard.CANONICAL
            (base / 'app').mkdir(parents=True)
            (base / 'app/build.gradle.kts').write_text('versionName = "changed"\nversionCode = 999')
            (base / 'tools/mods').mkdir(parents=True)
            source = guard.ROOT / guard.CANONICAL / 'tools/mods/pack.py'
            (base / 'tools/mods/pack.py').write_bytes(source.read_bytes())
            sample = base / 'samples/Sample/Playable'
            sample.mkdir(parents=True)
            (sample / 'manifest.json').write_text('{"version":"0.2.0"}')
            data = {'schemaVersion': 1,
                    'loader': {'versionName': 'old', 'versionCode': 1,
                               'sourceCommit': 'a' * 40, 'apkSha256': 'b' * 64},
                    'packages': [{'sample': 'Sample', 'version': '0.1.0', 'size': 0, 'sha256': '0' * 64}]}
            (root / 'docs').mkdir()
            (root / 'docs/checkpoint.json').write_text(json.dumps(data))
            errors = guard.check_checkpoint(root)
            self.assertEqual(len(errors), 4)
            self.assertTrue(any('versionName' in e for e in errors))
            self.assertTrue(any('versionCode' in e for e in errors))
            self.assertTrue(any('differs from manifest' in e for e in errors))
            self.assertTrue(any('package differs' in e for e in errors))


if __name__ == '__main__':
    unittest.main()
