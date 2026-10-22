import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('example_audit', ROOT/'tools/example_completion_audit.py')
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)

class EvidenceTests(unittest.TestCase):
    def test_actual_evidence_has_no_shadowed_examples(self):
        source = (ROOT/'tools/example_completion/evidence.json').read_text()
        data = json.loads(source, object_pairs_hook=AUDIT.unique_object)
        self.assertIn('Content/Biomes/ExampleUndergroundBackgroundStyle.js', data['examples'])

    def test_duplicate_example_cannot_hide_recorded_device_evidence(self):
        with self.assertRaisesRegex(ValueError, 'Duplicate evidence key'):
            json.loads('{"Example.js":{"deviceEvidence":["user report"]},"Example.js":{"deviceEvidence":[]}}',
                       object_pairs_hook=AUDIT.unique_object)
