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
assert "snapshotManagedPrefixes()" in game
assert "std::nearbyint" in game
assert "fields.size() == 16" in source
assert "weight != MathF.Truncate(weight)" in worker
assert "SourceParser" in (cpp / "SourceLoader.cpp").read_text()
assert "prefixes.size() != static_cast<size_t>(mod->prefixes)" in source
print("BUNNY_CSHARP_PREFIX_GAMEPLAY_WIRING=PASS android_runtime=not_tested")
