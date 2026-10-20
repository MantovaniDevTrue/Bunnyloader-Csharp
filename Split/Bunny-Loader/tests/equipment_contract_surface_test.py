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
        self.assertTrue(by_loader["PrefixLoader"]["RollChance"]["sdkNamePresent"])
        self.assertTrue(by_loader["PrefixLoader"]["CanRoll"]["sdkNamePresent"])
        self.assertFalse(by_loader["PrefixLoader"]["RollChance"]["semanticParityConfirmed"])
        self.assertFalse(by_loader["PrefixLoader"]["CanRoll"]["semanticParityConfirmed"])
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
        self.assertIn("if (want == -1) request = -2;", gameplay)

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
        self.assertIn("if (!accepted || !ready || want == -3)", gameplay)
        self.assertIn("damage > INT32_MAX - damageBonus", gameplay)
        self.assertIn("value > INT32_MAX - valueBonus", gameplay)
        self.assertIn("g_appliedItemPrefixes", gameplay)
        self.assertIn("currentDamage - previous->damageBonus", gameplay)
        self.assertIn("currentValue - previous->valueBonus", gameplay)
        self.assertIn("if (removedPrevious && !accepted)", gameplay)
        self.assertIn("rememberPrefix(item, type, prefix, appliedDamage, appliedValue", gameplay)
        fixture = (ROOT / "tests/prefix_v89/PrefixFixture/Content.cs").read_text(encoding="utf-8")
        self.assertIn("effects.DamageBonus = 4;", fixture)
        self.assertIn("effects.ValueBonus = 125;", fixture)
        self.assertIn('"PREFIX_POST"', compiler)
        self.assertIn('fields[0] == "PREFIX_POST"', source)
        self.assertIn("prefixPostPolicies.erase(post)", source)
        self.assertIn("const auto* prefixPolicy = prefixView ? byType(*prefixView, prefix)", gameplay)
        self.assertIn("append(selected->extraTooltip);", gameplay)
        self.assertIn("requireEffectiveStats", gameplay)
        self.assertIn("effects.DamageBonus = 3;", fixture)
        playable = (ROOT / "samples/ExampleModCSharp/Playable/content/Content/Prefixes/ExampleMightyPrefix.cs").read_text(encoding="utf-8")
        self.assertIn('rules.Allow("ExampleWand", 3);', playable)
        self.assertIn('ExtraTooltip = "Bonus adicional do prefixo";', playable)


    def test_item_owned_native_prefix_denial_contract(self):
        sdk = json.loads((ROOT / "docs/generated/sdk_public_reference.json").read_text(encoding="utf-8"))
        typed = {entry["name"]: entry for entry in sdk["types"]}
        for name in ("BunnyItem", "BunnyWeapon", "BunnyAccessory", "BunnyBuffPotion"):
            signatures = {method["signature"] for method in
                          typed["BunnyLoader.Source." + name]["methods"]}
            self.assertIn("void ConfigurePrefixRules(BunnyItemPrefixRules rules)", signatures)
        self.assertIn("BunnyLoader.Source.BunnyItemPrefixRules", typed)
        ordered = [entry["name"] for entry in sdk["types"]]
        self.assertEqual(ordered, sorted(ordered))
        for entry in ("BunnyLoader.Source.BunnyItemPrefixRules",
                      "BunnyLoader.Source.BunnyItem", "BunnyLoader.Source.BunnyWeapon",
                      "BunnyLoader.Source.BunnyAccessory", "BunnyLoader.Source.BunnyBuffPotion"):
            signatures = [method["signature"] for method in typed[entry]["methods"]]
            self.assertEqual(signatures, sorted(signatures))
        compiler = (ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text(encoding="utf-8")
        parser = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp").read_text(encoding="utf-8")
        hooks = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        self.assertIn('"PREFIX_DENY"', compiler)
        self.assertIn('fields[0] == "PREFIX_DENY"', parser)
        self.assertIn("applyManagedItemPrefixDenials(modId, prefixDenials)", parser)
        self.assertIn("std::atomic_store_explicit(&g_itemDenyView", bridge)
        self.assertIn("bool isPrefixDenied(Il2CppObject* item, int prefix)", hooks)
        self.assertIn("g_restoreRoll.item != item", hooks)
        self.assertIn("const std::set<int>* denied =", hooks)
        fixture = (ROOT / "tests/prefix_v89/PrefixFixture/Content.cs").read_text(encoding="utf-8")
        self.assertIn('rules.Deny("TestMightyPrefix");', fixture)
        playable = (ROOT / "samples/ExampleModCSharp/Playable/content/Content/Items/Weapons/ExampleWand.cs").read_text(encoding="utf-8")
        self.assertIn('rules.Deny(nameof(ExampleGuardedPrefix));', playable)
        self.assertIn("rules.Deny(71);", playable)

    def test_prefix_runtime_item_state_conditions_from_csharp_to_native(self):
        sdk = json.loads((ROOT / "docs/generated/sdk_public_reference.json").read_text(encoding="utf-8"))
        typed = {entry["name"]: entry for entry in sdk["types"]}
        prefix_methods = {method["signature"] for method in
                          typed["BunnyLoader.Source.BunnyPrefix"]["methods"]}
        self.assertIn("void ConfigureItemConditions(BunnyPrefixItemConditions conditions)", prefix_methods)
        condition_methods = {method["signature"] for method in
                             typed["BunnyLoader.Source.BunnyPrefixItemConditions"]["methods"]}
        self.assertIn("void RequireDamageBetween(int minimum, int maximum)", condition_methods)
        self.assertIn("void RequireManaBetween(int minimum, int maximum)", condition_methods)
        compiler = (ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text(encoding="utf-8")
        parser = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp").read_text(encoding="utf-8")
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        fixture = (ROOT / "tests/prefix_v89/PrefixFixture/Content.cs").read_text(encoding="utf-8")
        self.assertIn('"PREFIX_CONDITION"', compiler)
        self.assertIn('fields[0] == "PREFIX_CONDITION"', parser)
        self.assertIn("prefixItemConditions.erase(condition)", parser)
        self.assertIn("p.maxItemMana > 10000", bridge)
        self.assertIn("if (damage < p.minItemDamage || damage > p.maxItemDamage)", gameplay)
        self.assertIn("if (mana < p.minItemMana || mana > p.maxItemMana)", gameplay)
        self.assertIn("conditions.RequireDamageBetween(10, 25);", fixture)
        self.assertIn("conditions.RequireManaBetween(0, 8);", fixture)
        example = (ROOT / "samples/ExampleModCSharp/Playable/content/Content/Prefixes/ExampleMightyPrefix.cs").read_text(encoding="utf-8")
        self.assertIn("conditions.RequireDamageBetween(10, 25);", example)
        self.assertIn("conditions.RequireManaBetween(0, 8);", example)

    def test_saved_prefix_ignores_new_roll_predicates(self):
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        cold = (ROOT / "tests/managed_prefix_cold_tables.cpp").read_text(encoding="utf-8")
        self.assertIn("g_restoreRoll.item == item && g_restoreRoll.prefix == prefix", gameplay)
        self.assertIn("if (restoreCompatible(item, *target) && ensurePrefixTables(*descriptors))", gameplay)
        self.assertIn("if (!requested || !restoreCompatible(item, *requested) ||", gameplay)
        self.assertIn("if (!found || found->category != 5) return;", gameplay)
        self.assertIn("restoredDespiteNewConditions", cold)
        self.assertIn("assert(!hookCanRoll(&weapon.object, 99, nullptr));", cold)
        self.assertIn("assert(player.defense == 14);", cold)

    def test_native_prefix_tooltip_multiple_lines_and_capacity(self):
        sdk = json.loads((ROOT / "docs/generated/sdk_public_reference.json").read_text(encoding="utf-8"))
        by_type = {entry["name"]: entry for entry in sdk["types"]}
        methods = {entry["signature"] for entry in
                   by_type["BunnyLoader.Source.BunnyPrefix"]["methods"]}
        self.assertIn("void AddTooltipLine(string text)", methods)
        self.assertIn("string SerializeTooltipLines()", methods)
        compiler = (ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text(encoding="utf-8")
        source = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp").read_text(encoding="utf-8")
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        fixture = (ROOT / "tests/prefix_v89/PrefixFixture/Content.cs").read_text(encoding="utf-8")
        self.assertIn('"PREFIX_TOOLTIP"', compiler)
        self.assertIn('fields[0] == "PREFIX_TOOLTIP"', source)
        self.assertIn("prefixTooltipPolicies.erase(lines)", source)
        self.assertIn("p.tooltipLines.size() + p.tooltipItemLines.size() > 4", bridge)
        self.assertIn("void AddTooltipItemLine(string template)", methods)
        self.assertIn("string SerializeTooltipItemLines()", methods)
        self.assertIn('"PREFIX_TOOLTIP_ITEM"', compiler)
        self.assertIn('fields[0] == "PREFIX_TOOLTIP_ITEM"', source)
        self.assertIn("prefixItemTooltipPolicies.erase(itemLines)", source)
        self.assertIn("validPrefixItemTooltip(line)", bridge)
        self.assertIn("renderPrefixItemTooltip(itemLine, damage, mana, stack)", gameplay)
        self.assertIn('AddTooltipItemLine("Dano {damage}, mana {mana}, pilha {stack}")', fixture)
        self.assertIn("for (const auto& line : selected->tooltipLines)", gameplay)
        self.assertIn("static_cast<uintptr_t>(*count) >= capacity", gameplay)
        self.assertIn('AddTooltipLine("First line")', fixture)
        self.assertIn('AddTooltipLine("Second line")', fixture)

    def test_mixed_mod_prefix_rolls_use_native_weights_and_denials(self):
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        fixture = (ROOT / "tests/managed_prefix_cold_tables.cpp").read_text(encoding="utf-8")
        self.assertIn("if (!hasManaged && !hasDeny && (!hasItemWorker || !filteredByItemCallback))", gameplay)
        self.assertNotIn("|| hasOtherMod)", gameplay)
        self.assertIn("weightedPrefixCandidate(", gameplay)
        self.assertIn("mixedPrefixCandidates = true;", fixture)
        self.assertIn('type == 100 ? "other/External"', fixture)
        self.assertIn("guarded.rollWeight = 3;", fixture)
        self.assertIn("assert(mixed && mixed->length == 5);", fixture)
        self.assertIn("assert(mixedFiltered && mixedFiltered->length == 4);", fixture)
        self.assertIn("assert(nativeRolls == priorNativeRolls);", fixture)

    def test_native_prefix_qa_telemetry_contract(self):
        source = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        header = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.h").read_text(encoding="utf-8")
        debug = (ROOT / "app/src/main/cpp/script/api/CSharpPrototypeApi.cpp").read_text(encoding="utf-8")
        self.assertIn("struct ManagedPrefixDiagnostics", header)
        self.assertIn("ManagedPrefixDiagnostics managedPrefixDiagnostics()", source)
        self.assertIn("std::memory_order_relaxed", source)
        self.assertIn("g_prefixRollMaxUs.compare_exchange_weak", source)
        self.assertIn('"nativePrefixDiagnostics"', debug)
        self.assertIn("jsNativePrefixDiagnostics", debug)
        self.assertNotIn("invokeManagedRecipe", source)
        self.assertNotIn("invokeManagedShoot", source)

    def test_prefix_worker_transport_contract(self):
        runtime = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text(encoding="utf-8")
        header = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedRuntime.h").read_text(encoding="utf-8")
        callbacks = (ROOT / "experiments/csharp/BunnyCsSdk/BunnyPrefixCallbacks.cs").read_text(encoding="utf-8")
        sdk = (ROOT / "experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj").read_text(encoding="utf-8")
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        self.assertIn("bool invokeManagedPrefix(", runtime)
        self.assertIn("bool invokeManagedPrefix(", header)
        self.assertIn('job.prefix?"BunnyPrefixRuntime"', runtime)
        self.assertIn("g_recipeRpcQueue.push_back({mod,assembly,owner,snapshot,index,kind,until,completion,shooting,tile,prefix,itemPrefix})", runtime)
        self.assertIn("completion->cancelled=true;", runtime)
        self.assertIn("!job.completion->cancelled&&std::chrono::steady_clock::now()<job.deadline", runtime)
        self.assertIn("BunnyPrefixItemSnapshot", callbacks)
        self.assertIn("RuntimeHelpers.PrepareMethod(callback.MethodHandle)", callbacks)
        self.assertIn("BunnyPrefixCallbacks.cs", sdk)
        self.assertIn('if (kind == 2) return *output == "READY";', runtime)
        self.assertIn('if (kind == 0 || kind == 3) return *output == "0" || *output == "1";', runtime)
        self.assertIn('return weight >= 0 && weight <= 20;', runtime)
        self.assertIn('kind == 0 || kind == 3', runtime)
        self.assertIn('if (kind == 4)', runtime)
        self.assertIn('if (kind == 5)', runtime)
        self.assertIn('if (kind == 6)', runtime)
        self.assertIn('queryPrefixAccessory(player,*found)', gameplay)
        self.assertIn('now-found.checked<std::chrono::milliseconds(250)', gameplay)
        self.assertIn('now-lastRequest<std::chrono::milliseconds(150)', gameplay)
        self.assertIn('managedPrefixWarmupState(p.modId,assembly->second,p.name)', gameplay)
        self.assertIn('p.callbackMask<0 || p.callbackMask>63', (
            ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp"
        ).read_text(encoding="utf-8"))
        self.assertIn('kind>(prefix?6:3)', runtime)
        self.assertIn('itemPrefix ? ', runtime)
        self.assertNotIn('kind<0||kind>3||', runtime)
        self.assertIn('queryPrefixTooltip(item,*selected)', gameplay)
        self.assertIn('queryPrefixTooltip(item,*p)', gameplay)
        self.assertIn('if (!defense && !dynamic) return;', gameplay)
        self.assertIn('now-cache.updated<std::chrono::milliseconds(200)', gameplay)
        self.assertIn('queryPrefixApply(item,*prefixPolicy', gameplay)
        self.assertIn('queryPrefixWorker(item,*p,3)', gameplay)
        self.assertIn('if (!success) return false;', runtime)
        self.assertIn("invokeManagedPrefix(", gameplay)
        self.assertIn("PrefixRpcScope prefixCallbacks;", gameplay)
        self.assertIn("managedPrefixWarmupState(", gameplay)
        self.assertIn("prefix.callbackMask = callback->second;", (
            ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp"
        ).read_text(encoding="utf-8"))
        self.assertIn('PREFIX_CALLBACK', (
            ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs"
        ).read_text(encoding="utf-8"))

    def test_item_prefix_worker_is_bounded_and_bound_before_gameplay(self):
        runtime = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp").read_text(encoding="utf-8")
        auto = (ROOT / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text(encoding="utf-8")
        self.assertIn('job.itemPrefix?"BunnyItemPrefixRuntime"', runtime)
        self.assertIn('g_itemPrefixWarmups.emplace(key,completion)', runtime)
        self.assertIn('completion->cancelled=true;', runtime)
        self.assertIn('(*output=="0" || *output=="1")', runtime)
        self.assertIn('g_itemCallbackAssemblies', bridge)
        self.assertIn('queueManagedItemPrefixWarmup(modId,assembly,owner.owner)', bridge)
        self.assertIn('bindManagedItemPrefixCallbackAssembly(mod.id,assembly)', auto)

    def test_item_prefix_callback_wire_is_registered_without_live_gameplay_hook(self):
        compiler = (ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text(encoding="utf-8")
        loader = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text(encoding="utf-8")
        gameplay = (ROOT / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text(encoding="utf-8")
        self.assertIn('string.Join("\\t", "ITEM_PREFIX_CALLBACK", itemType.Name, "1")', compiler)
        self.assertIn('fields[0] == "ITEM_PREFIX_CALLBACK"', loader)
        self.assertIn('seenItemPrefixCallbacks.insert(registered->second).second', loader)
        self.assertIn('invokeManagedItemPrefix(', gameplay)
        self.assertIn('managedItemPrefixWarmupState(', gameplay)
        self.assertIn('g_restoreRoll.item==item', gameplay)
        self.assertIn('scope->requests>=4', gameplay)
        self.assertIn('itemAllowsPrefix(item,choice)', gameplay)
        self.assertIn('itemAllowsPrefix(item,prefix)', gameplay)

    def test_report_render_is_stable_and_honest(self):
        markdown = surface.render_markdown(surface.audit(ROOT))
        self.assertEqual(markdown, surface.render_markdown(surface.audit(ROOT)))
        self.assertIn("não equivalência", markdown)
        self.assertIn("Nenhum loader é promovido a completo", markdown)
        for loader in surface.GROUPS:
            self.assertIn("## " + loader, markdown)


if __name__ == "__main__":
    unittest.main()
