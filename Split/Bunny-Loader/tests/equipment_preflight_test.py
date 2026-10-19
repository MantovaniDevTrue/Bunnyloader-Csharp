import copy
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "tools/equipment_preflight.py"
SPEC = importlib.util.spec_from_file_location("equipment_preflight", MODULE)
preflight = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(preflight)


class EquipmentPreflightTests(unittest.TestCase):
    def test_report_matches_next_batch_without_claiming_parity(self):
        report = preflight.build_report(ROOT)
        self.assertEqual(report["batch"], "loaders-02")
        self.assertEqual([entry["loader"] for entry in report["loaders"]],list(preflight.SPECS))
        self.assertEqual(len(report["loaders"]), 5)
        for entry in report["loaders"]:
            self.assertTrue(entry["modules"])
            self.assertTrue(entry["originalExamples"])
            self.assertTrue(entry["existingCSharpExamples"])
            self.assertTrue(entry["reviewRequired"])
            self.assertFalse(entry["deviceVerifiedByThisReport"])
            self.assertTrue(all(entry["sdkTypes"].values()))
            self.assertTrue(all(entry["originalSymbols"].values()))
            for file in entry["modules"] + entry["originalExamples"] + entry["existingCSharpExamples"]:
                self.assertEqual(len(file["sha256"]), 64)
                self.assertEqual(preflight.sha256(ROOT / file["path"]),file["sha256"])

    def test_missing_original_fails_without_mutating_sources(self):
        specs = copy.deepcopy(preflight.SPECS)
        specs["EquipLoader"]["originals"] = ("Content/Items/Accessories/DoesNotExist.js",)
        with self.assertRaisesRegex(ValueError, "Source missing"):
            preflight.build_report(ROOT,specs)

    def test_missing_port_fails_without_marking_loader_complete(self):
        specs = copy.deepcopy(preflight.SPECS)
        specs["PrefixLoader"]["ports"] = ("Content/Prefixes/DoesNotExist.cs",)
        with self.assertRaisesRegex(ValueError, "Source missing"):
            preflight.build_report(ROOT,specs)

    def test_markdown_is_deterministic_and_explicitly_unverified(self):
        left = preflight.as_markdown(preflight.build_report(ROOT))
        right = preflight.as_markdown(preflight.build_report(ROOT))
        self.assertEqual(left,right)
        self.assertIn("não certifica",left.lower())
        for name in preflight.SPECS:
            self.assertIn(name,left)


if __name__ == "__main__":
    unittest.main()
