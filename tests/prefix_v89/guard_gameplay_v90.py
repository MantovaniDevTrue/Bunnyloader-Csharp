from pathlib import Path

root = Path(__file__).resolve().parents[2]
cpp = root / "app/src/main/cpp/experiments/csharp"
game = (cpp / "ManagedPrefixGameplay.cpp").read_text()
bridge = (cpp / "ManagedPrefixBridge.cpp").read_text()
source = (cpp / "SourceLoader.cpp").read_text()
worker = (root / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text()

for hook in ("GetRollablePrefixes()", "CanRollPrefix(int prefix)",
             "TryGetPrefixStatMultipliersForItem(", "GrantPrefixBenefits(Item item)"):
    assert hook in game, hook
for name in ("hookRollable", "hookCanRoll", "hookStats", "hookGrant"):
    assert name in game
assert "TypeTables::resizedCopy" in game
assert "content::makeLocalizedText" in game
assert "registeredManagedPrefixCount" in bridge
assert "installManagedPrefixGameplayHooks()" in bridge
assert "managedPrefixView()" in game
assert "std::nearbyint" in game
assert "PREFIX_STACK_WEIGHT" in source
assert "PREFIX_ATTRIBUTE_WEIGHT" in source
assert "prefix.hasAttributeRollWeight = true;" in source
assert "p.hasAttributeRollWeight" in game
assert "OverrideRollWeightWhenManaBetween" in (root / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text()
assert "PREFIX_DENY_STACK" in source
assert "PREFIX_TOOLTIP_ITEM" in source
assert "renderPrefixItemTooltip(itemLine, damage, mana, stack)" in game
assert "PREFIX_DENY_ATTRIBUTE" in source
assert "applyManagedItemPrefixAttributeDenials" in source
assert "managedItemPrefixAttributeDenyView()" in game
assert "PREFIX_CHANCE_STACK" in source
assert "PREFIX_CHOOSE_STACK" in source
runtime = (cpp / "ManagedRuntime.cpp").read_text()
runtime_header = (cpp / "ManagedRuntime.h").read_text()
callbacks = (root / "experiments/csharp/BunnyCsSdk/BunnyPrefixCallbacks.cs").read_text()
assert "bool invokeManagedPrefix(" in runtime
assert "bool invokeManagedPrefix(" in runtime_header
assert 'job.prefix?"BunnyPrefixRuntime"' in runtime
assert "g_recipeRpcQueue.push_back({mod,assembly,owner,snapshot,index,kind,until,completion,shooting,tile,rarity,prefix,itemPrefix})" in runtime
assert "completion->cancelled=true;" in runtime
assert "job.completion->cancelled||std::chrono::steady_clock::now()>=job.deadline" in runtime
assert "BunnyPrefixItemSnapshot" in callbacks
assert 'public static string Dispatch(string modId,string assemblyPath,string owner,' in callbacks
assert "RuntimeHelpers.PrepareMethod(callback.MethodHandle)" in callbacks
assert '("AllStatChangesHaveEffectOn", 4)' in worker
assert "callbackMask |= 8" in worker
assert "queryPrefixApply(item,*prefixPolicy" in game
assert "queryPrefixWorker(item,*p,3)" in game
assert "ManagedPrefixDiagnostics managedPrefixDiagnostics()" in game
assert "g_prefixRestoredRolls.fetch_add" in game
assert "g_prefixChosenRolls.fetch_add" in game
assert "g_prefixWeightedRolls.fetch_add" in game
assert "g_prefixVanillaFallbacks.fetch_add" in game
assert "applyManagedItemPrefixChoices" in source
assert "managedItemPrefixChoiceView()" in game
assert "applyManagedItemPrefixStackChances" in source
assert "managedItemPrefixStackChanceView()" in game
assert "applyManagedItemPrefixStackDenials" in source
assert "managedItemPrefixStackDenyView()" in game
assert "prefix.hasStackRollWeight = true;" in source
assert "p.hasStackRollWeight" in game
assert "stack >= p.minWeightStack && stack <= p.maxWeightStack" in game
assert "OverrideRollWeightWhenStackBetween" in (root / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text()
assert 'r.stack = fieldOffset(item, "stack");' in game
assert "if (p.minItemStack != 0 || p.maxItemStack != 999999)" in game
assert "fields.size() != 6 && fields.size() != 8" in source
assert "prefix.maxItemStack = condition->second.maxItemStack;" in source
assert "fields.size() == 16" in source
assert "weight != MathF.Truncate(weight)" in worker
assert "SourceParser" in (cpp / "SourceLoader.cpp").read_text()
assert "prefixes.size() != static_cast<size_t>(mod->prefixes)" in source
print("BUNNY_CSHARP_PREFIX_GAMEPLAY_WIRING=PASS android_runtime=not_tested")
