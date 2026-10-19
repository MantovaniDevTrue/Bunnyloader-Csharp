import copy
import importlib.util
import json
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

    def test_compiled_prefix_canroll_policy_is_documented_without_claiming_live_callbacks(self):
        sdk = json.loads((ROOT / "docs/generated/sdk_public_reference.json").read_text(encoding="utf-8"))
        by_type = {entry["name"]: entry for entry in sdk["types"]}
        rules = by_type["BunnyLoader.Source.BunnyPrefixRollRules"]
        signatures = {entry["signature"]: entry["execution"] for entry in rules["methods"]}
        for signature in (
            "void PreferFor(int itemType)",
            "void PreferFor(string itemName)",
            "void SetCanRoll(int itemType, bool allowed)",
            "void SetCanRoll(string itemName, bool allowed)",
            "void SetWeight(int itemType, int weight)",
            "void SetWeight(string itemName, int weight)",
        ):
            self.assertEqual(signatures[signature], "compile_policy")
        prefix = by_type["BunnyLoader.Source.BunnyPrefix"]
        self.assertIn(
            "void ConfigureRollRules(BunnyPrefixRollRules rules)",
            {entry["signature"] for entry in prefix["methods"]},
        )
        source = (ROOT / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text(encoding="utf-8")
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        self.assertIn("public void SetCanRoll(int itemType, bool allowed)", source)
        self.assertIn("public void SetCanRoll(string itemName, bool allowed)", source)
        self.assertIn("bool itemCanRoll(Il2CppObject* item", gameplay)
        self.assertIn("if (!item || !itemCanRoll(item, p)) return false;", gameplay)

    def test_item_prefix_chance_contract_stays_item_owned_and_natively_gated(self):
        sdk = json.loads((ROOT / "docs/generated/sdk_public_reference.json").read_text(encoding="utf-8"))
        by_type = {entry["name"]: entry for entry in sdk["types"]}
        for name in ("BunnyItem", "BunnyWeapon", "BunnyAccessory", "BunnyBuffPotion"):
            self.assertIn(
                "Nullable<bool> PrefixChance { get; set; }",
                by_type["BunnyLoader.Source." + name]["properties"],
            )
        compiler = (ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text(encoding="utf-8")
        source = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp").read_text(encoding="utf-8")
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        self.assertIn('"PREFIX_CHANCE"', compiler)
        self.assertIn('fields[0] == "PREFIX_CHANCE"', source)
        self.assertIn('(basic || weapon || accessory || potion) && key == "PrefixChance"', source)
        self.assertIn('PrefixChance requer literal true/false', source)
        self.assertIn("seenPrefixChances.insert", source)
        self.assertIn("applyManagedItemPrefixChances(prefixChances)", source)
        self.assertIn("std::atomic_store_explicit(&g_itemChanceView", bridge)
        self.assertIn("bool hookItemPrefixChance(", gameplay)
        self.assertIn("g_restoreRoll.item == item", gameplay)
        self.assertIn("want == -1 ? -2 : want", gameplay)

    def test_item_apply_prefix_policy_is_sampled_and_native_post_success(self):
        sdk = json.loads((ROOT / "docs/generated/sdk_public_reference.json").read_text(encoding="utf-8"))
        by_type = {entry["name"]: entry for entry in sdk["types"]}
        state = by_type["BunnyLoader.Source.BunnyPrefixApplyStats"]
        self.assertEqual(state["properties"], [
            "int DamageBonus { get; set; }", "int ValueBonus { get; set; }",
        ])
        for name in ("BunnyItem", "BunnyWeapon", "BunnyAccessory", "BunnyBuffPotion"):
            signatures = {method["signature"]: method["execution"]
                          for method in by_type["BunnyLoader.Source." + name]["methods"]}
            self.assertEqual(signatures["void ApplyPrefix(ref BunnyPrefixApplyStats effects)"],
                             "compile_policy")
        compiler = (ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text(encoding="utf-8")
        source = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp").read_text(encoding="utf-8")
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        self.assertIn('"PREFIX_APPLY"', compiler)
        self.assertIn('fields[0] == "PREFIX_APPLY"', source)
        self.assertIn("applyManagedItemPrefixEffects(prefixEffects)", source)
        self.assertIn("std::atomic_store_explicit(&g_itemApplyView", bridge)
        self.assertIn("const bool accepted = g_originalItemPrefix(item, request, topTier, method);", gameplay)
        self.assertIn("if (!accepted || !item || want == -3", gameplay)
        self.assertIn("damage > INT32_MAX - policy->second.damageBonus", gameplay)
        self.assertIn("value > INT32_MAX - policy->second.valueBonus", gameplay)

    def test_report_render_is_stable_and_honest(self):
        markdown = surface.render_markdown(surface.audit(ROOT))
        self.assertEqual(markdown, surface.render_markdown(surface.audit(ROOT)))
        self.assertIn("não equivalência", markdown)
        self.assertIn("Nenhum loader é promovido a completo", markdown)
        for loader in surface.GROUPS:
            self.assertIn("## " + loader, markdown)


if __name__ == "__main__":
    unittest.main()
