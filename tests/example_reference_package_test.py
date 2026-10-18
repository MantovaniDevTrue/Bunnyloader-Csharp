"""Execute archive inventory contracts without executing JavaScript or QuickJS."""
import json
import sys
import tempfile
import unittest
import warnings
from pathlib import Path
from zipfile import ZipFile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from example_completion.reference_package import inventory
from example_completion_audit import facts


class ReferencePackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.old=self.root/'samples/ExampleMod/content'
        self.old.mkdir(parents=True)
        self.package=self.root/'example.bl'

    def make(self,files):
        with ZipFile(self.package,'w') as archive:
            archive.writestr('manifest.json',json.dumps({'version':'test'}))
            for name,body in files:
                archive.writestr(name,body)

    def test_retains_sources_and_facts_for_new_and_changed_examples(self):
        (self.old/'Old.js').write_text('class Old extends ModItem {}\n\n')
        (self.old/'Changed.js').write_text('class Changed extends ModItem {}')
        self.make([('content/Old.js','class Old extends ModItem {}\n'),
                   ('content/Changed.js','class Changed extends ModItem { AI() { return 1; } }'),
                   ('content/New.js','class New extends GlobalBuff {\n Update() { throw Error(); }\n}'),
                   ('content/New.png',b'asset')])
        result=inventory(self.package,self.root,facts)
        rows={entry['id']:entry for entry in result['examples']}
        self.assertEqual(rows['Old.js']['comparison'],'Unchanged')
        self.assertNotIn('source',rows['Old.js'])
        self.assertEqual(rows['Changed.js']['comparison'],'Changed')
        self.assertEqual(rows['New.js']['comparison'],'Added')
        self.assertIn('Update',rows['New.js']['facts']['hooksAndMethods'])
        self.assertIn('throw Error()',rows['New.js']['source'])
        self.assertEqual(len(result['files']),4)
        self.assertEqual(result,inventory(self.package,self.root,facts))

    def test_rejects_traversal_and_windows_paths(self):
        for bad in ('../escape.js','/content/escape.js','content/../escape.js','content\\escape.js'):
            self.make([(bad,'danger')])
            with self.assertRaisesRegex(ValueError,'Unsafe reference path'):
                inventory(self.package,self.root,facts)

    def test_rejects_duplicate_paths(self):
        with warnings.catch_warnings():
            warnings.simplefilter('ignore',UserWarning)
            self.make([('content/A.js','first'),('content/A.js','second')])
        with self.assertRaisesRegex(ValueError,'duplicate paths'):
            inventory(self.package,self.root,facts)

    def test_committed_snapshot_keeps_every_pinned_example(self):
        spec=ROOT/'tools/example_completion'
        pinned=json.loads((spec/'references.json').read_text())
        snapshot=json.loads((spec/'package-reference.json').read_text())
        old={entry['path'].removeprefix('samples/ExampleMod/content/')
             for entry in pinned['upstreamExampleFiles'] if entry['path'].endswith('.js')}
        current=[entry['id'] for entry in snapshot['examples']]
        self.assertEqual(len(current),len(set(current)))
        self.assertTrue(old<=set(current),'An existing example disappeared')
        matrix=json.loads((ROOT/'docs/generated/example_completion/matrix.json').read_text())
        self.assertEqual(set(current),{entry['id'] for entry in matrix['examples']})
        for entry in snapshot['examples']:
            if entry['comparison']!='Unchanged':
                self.assertEqual(facts(entry['source']),entry['facts'])


if __name__=='__main__':
    unittest.main()
