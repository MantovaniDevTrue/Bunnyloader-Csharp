import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('readiness', ROOT / 'tools/ci/check_examplemod_apk_readiness.py')
readiness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(readiness)


class ApkReadiness(unittest.TestCase):
    def setUp(self):
        self.reference = {'examples': [{'id': 'Content/Example.js', 'facts': {'hooksAndMethods': ['SetDefaults', 'AI']}}]}
        self.coverage = {'behaviors': [dict(id='Content/Example.js#' + hook,
            status='validado automaticamente', implementation=['native.cpp'],
            tests=['test.cpp'], validatedFilesSha256={'native.cpp': 'hash'})
            for hook in ['SetDefaults', 'AI']]}

    def test_completed_host_contracts_can_reach_android_qa(self):
        self.assertEqual([], readiness.blockers(self.coverage, self.reference))

    def test_pending_failed_blocked_and_untested_features_block_packaging(self):
        for status in ['não iniciado', 'implementado sem validar', 'falhou', 'bloqueio comprovado', 'desconhecido']:
            self.coverage['behaviors'][0]['status'] = status
            self.assertTrue(readiness.blockers(self.coverage, self.reference), status)

    def test_deleting_pending_row_does_not_unlock_build(self):
        self.coverage['behaviors'].pop()
        self.assertIn('Content/Example.js#AI: sem registro de tradução', readiness.blockers(self.coverage, self.reference))

    def test_validation_without_implementation_tests_or_hashes_is_rejected(self):
        for key in ['implementation', 'tests', 'validatedFilesSha256']:
            old = self.coverage['behaviors'][0].pop(key)
            self.assertTrue(readiness.blockers(self.coverage, self.reference), key)
            self.coverage['behaviors'][0][key] = old

    def test_android_claim_needs_device_evidence(self):
        self.coverage['behaviors'][0]['status'] = 'validado no Android'
        self.assertTrue(readiness.blockers(self.coverage, self.reference))
        self.coverage['behaviors'][0]['androidEvidence'] = [{'source': 'device log'}]
        self.assertEqual([], readiness.blockers(self.coverage, self.reference))

    def test_empty_reference_and_duplicate_ids_do_not_unlock_build(self):
        self.assertTrue(readiness.blockers(self.coverage, {'examples': []}))
        self.coverage['behaviors'].append(self.coverage['behaviors'][0])
        self.assertTrue(readiness.blockers(self.coverage, self.reference))

    def test_global_requirements_also_block_packaging(self):
        self.coverage['behaviors'].append({'id': 'requirement:tooltip-without-js', 'status': 'falhou'})
        self.assertTrue(readiness.blockers(self.coverage, self.reference))


if __name__ == '__main__':
    unittest.main()
