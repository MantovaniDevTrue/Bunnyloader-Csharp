import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('delivery_gate', ROOT/'tools/ci/check_loader_delivery_gate.py')
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)

class GateTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads((ROOT/'docs/csharp-loader-completion-15.json').read_text())

    def test_real_scope_blocks_apk(self):
        result = subprocess.run(['python3',str(ROOT/'tools/ci/check_loader_delivery_gate.py'),'--root',str(ROOT)],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('APK_BLOCKED_INCOMPLETE_LOADER_SCOPE',result.stdout)
        self.assertIn('WaterStyleLoader: implementation=pending',result.stdout)

    def test_omitting_or_duplicating_a_loader_does_not_unlock_delivery(self):
        del self.manifest['loaders'][-1]
        self.assertTrue(GATE.blockers(self.manifest,ROOT))
        self.manifest['loaders'].append(copy.deepcopy(self.manifest['loaders'][0]))
        self.assertTrue(GATE.blockers(self.manifest,ROOT))

    def test_claiming_implemented_without_evidence_does_not_unlock(self):
        for row in self.manifest['loaders']:
            row['implementation']='implemented'
            row['missing']=[]
        self.assertTrue(GATE.blockers(self.manifest,ROOT))

    def test_complete_fixture_preserves_checks_for_missing_contract_tests(self):
        row=self.manifest['loaders'][0]
        row['contracts'][0]['tests']=['missing-test.cpp']
        self.assertTrue(any('test missing' in e for e in GATE.blockers(self.manifest,ROOT)))
