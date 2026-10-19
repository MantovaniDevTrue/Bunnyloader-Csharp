import copy
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "tools/equipment_contract_surface.py"
SPEC = importlib.util.spec_from_file_location("equipment_contract_surface", MODULE)
surface = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(surface)


class EquipmentContractSurfaceTests(unittest.TestCase):
    def test_expected_callbacks_anchor_to_original_sources(self):
        report = surface.audit(ROOT)
        self.assertEqual(len(report["loaders"]), 5)
        self.assertEqual([entry["loader"] for entry in report["loaders"]],
                         list(surface.GROUPS))
        expected = sum(len(g["callbacks"]) for g in surface.GROUPS.values())
        self.assertEqual(sum(len(loader["callbacks"]) for loader in report["loaders"]), expected)
        self.assertEqual(len(report["sdkReferenceSha256"]), 64)
        self.assertIn("never proof", report["meaning"])
        for loader in report["loaders"]:
            self.assertEqual(len(loader["originalSha256"]), 64)
            self.assertTrue((ROOT / loader["originalSource"]).is_file())
            for callback in loader["callbacks"]:
                self.assertGreater(callback["originalLine"], 0)
                self.assertFalse(callback["semanticParityConfirmed"])
                self.assertTrue(callback["requiresNativeAndDeviceReview"])

    def test_sdk_overlap_is_lexical_not_parity(self):
        by_loader = {row["loader"]: {c["original"]: c for c in row["callbacks"]}
                     for row in surface.audit(ROOT)["loaders"]}
        self.assertFalse(by_loader["PrefixLoader"]["RollChance"]["sdkNamePresent"])
        self.assertFalse(by_loader["PrefixLoader"]["CanRoll"]["sdkNamePresent"])
        self.assertTrue(by_loader["PrefixLoader"]["SetStats"]["sdkNamePresent"])
        self.assertTrue(by_loader["PrefixLoader"]["ModifyValue"]["sdkNamePresent"])
        self.assertFalse(by_loader["WingLoader"]["WingUpdate"]["sdkNamePresent"])
        self.assertTrue(by_loader["WingLoader"]["SetWingStats"]["sdkNamePresent"])

    def test_source_api_drift_fails_loudly(self):
        alternate = copy.deepcopy(surface.GROUPS)
        alternate["PrefixLoader"]["callbacks"] += ("ImaginaryCallback",)
        with self.assertRaisesRegex(ValueError, "original callback missing"):
            surface.audit(ROOT, alternate)
        alternate = copy.deepcopy(surface.GROUPS)
        alternate["PrefixLoader"]["types"] = ("ImaginarySdkType",)
        with self.assertRaisesRegex(ValueError, "Missing SDK type"):
            surface.audit(ROOT, alternate)

    def test_report_render_is_stable_and_honest(self):
        markdown = surface.render_markdown(surface.audit(ROOT))
        self.assertEqual(markdown, surface.render_markdown(surface.audit(ROOT)))
        self.assertIn("não equivalência", markdown)
        self.assertIn("Nenhum loader é promovido a completo", markdown)
        for loader in surface.GROUPS:
            self.assertIn("## " + loader, markdown)


if __name__ == "__main__":
    unittest.main()
