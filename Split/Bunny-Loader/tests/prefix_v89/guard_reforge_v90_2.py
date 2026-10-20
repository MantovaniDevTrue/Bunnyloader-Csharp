from pathlib import Path

root = Path(__file__).resolve().parents[2]
game = (root / "app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text()
js = (root / "app/src/main/cpp/script/js/mod/Loaders/PrefixLoader.js").read_text()
header = (root / "app/src/main/cpp/experiments/csharp/ManagedPrefixRollSelection.h").read_text()

assert "bool RollAPrefix(UnifiedRandom random, ref int rolledPrefix)" in js
assert "bool RollAPrefix(UnifiedRandom random, ref int rolledPrefix)" in game
assert "hookRoll(Il2CppObject* item, Il2CppObject* random" in game
assert "hookRoll, &g_originalRoll" in game
assert "weightedPrefixCandidate" in game
assert "r.maxStack" not in game
assert "maxStack) != 1" not in game
assert "field<int32_t>(item, r.damage) <= 0" in game
assert "field<int32_t>(item, r.ammo) != 0" in game
assert "field<uint8_t>(item, r.consumable)" in game
assert "hasOtherMod" not in game
assert "thread_local Il2CppObject* g_failedRollableItem = nullptr;" in game
assert "if (total > 512 || !g_refs.intClass)" in game
assert game.count("g_failedRollableItem = item;") == 3
assert "if (!tablesReady)" in game
assert "eligible(item, p, vanillaHasRollable) && itemRollWeight(item, p) > 0" in game
assert "const bool managedCandidateFailure = g_failedRollableItem == item;" in game
assert "return (hasDeny || managedCandidateFailure) ? false :" in game
assert "if (!hasManaged && !hasDeny && (!hasItemWorker || !filteredByItemCallback))" in game
assert "for (uintptr_t i = 0; i < list->length; ++i)" in game
assert "g_originalRoll(item, random, rolled, method)" in game
assert "std::isfinite(draw)" in header
print("BUNNY_CSHARP_PREFIX_REFORGE_ROLL_NATIVE=PASS game_runtime=not_tested")
