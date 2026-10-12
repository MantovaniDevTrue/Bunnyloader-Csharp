# Bunny Loader C# .90 — native prefix gameplay prototype

This experiment extends .89 with native C++ hooks for a subset of the Terraria prefix lifecycle. Mod authors write prefix declarations and callback logic in C# only, the MonoVM verifier evaluates their supported deterministic callbacks during load, and C++ applies the resulting native descriptors. No user-written JavaScript is required.

Hooks installed on verified signatures:
- Terraria.Item.GetRollablePrefixes: adds C# prefix IDs matching weapon or accessory category.
- Terraria.Item.CanRollPrefix: accepts a compatible managed prefix ID.
- Terraria.Item.TryGetPrefixStatMultipliersForItem: applies C# damage, knockback, speed, scale, mana, crit, tag, penetration and price outputs.
- Terraria.Player.GrantPrefixBenefits: adds C# prefix defense to equipped accessory.

The native integration also extends the game's Lang.prefix and PrefixID.Sets.ReducedNaturalChance tables with TypeTables::resizedCopy and registers names using ContentAssets.makeLocalizedText. Identity and the existing .bl save key use ModPrefixes rather than ephemeral index values.

Current restrictions: category Custom is not supported; RollWeight must be a positive integer between 1 and 20 and is encoded as repeated candidates in GetRollablePrefixes, pending Android verification that Terraria's original roll implementation treats repetitions as weighted draws. Item-specific C# CanRoll/Apply, tooltip callbacks and full inheritance are not implemented. The exact method hooks and fields are resolved at runtime and fail closed when unavailable.

CI compiles the C# callbacks, runs the managed verifier, validates native descriptors and runs C++20 syntax checks on the gameplay hook sources. None of these prove the hooks work on the Android game, so this is not a validated APK. The .86 Android baseline is unaffected. Verify reforging, prefix names, weapon damage, accessory defense, character saves and mixed JS+C# prefix interactions on device before treating .90 as usable.
