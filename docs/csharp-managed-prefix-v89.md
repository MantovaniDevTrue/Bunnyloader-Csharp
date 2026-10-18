# Bunny Loader C# .89 — Native prefix bridge staging

This branch contains an experimental end-to-end prefix declaration and MonoVM descriptor handoff, not a finished gameplay hook.

The C# source owns prefix Category, RollWeight, SetStats, ModifyValue and ApplyAccessoryEffects. MonoVM executes the C# methods once during managed assembly verification, converts their results to a typed TSV descriptor, then native SourceLoader validates its identity and passes it to ManagedPrefixBridge. That bridge uses the existing ModPrefixes registry, respecting the byte-sized prefix ID and save key based on mod/name.

A source mod with weapon/projectile content may include BunnyPrefix classes directly in .cs files. No .js file is generated or required for these classes.

**Not implemented yet:** adding prefixes to item reforging and weighted roll lists; returning the new multipliers from Terraria's TryGetPrefixStatMultipliersForItem; applying prefix defense to equipped accessories; names/tooltips; runtime item-dependent CanRoll and AllStatChangesHaveEffectOn; proper prefix-only mod source handling; gameplay save/load and multiplayer verification.

Do not label BunnyPrefix fully supported in the public API catalog and do not distribute .89 as a validated Android APK. The negative contract continues rejecting the not-yet-supported tooltip method, rather than rejecting the class name.

CI checks the real C# callback execution, managed/native descriptor layout, and unsupported API rejection. Android hooks and UI are not tested by these checks.

Original data path: ExampleModBunny JS source -> new C# managed callbacks -> compiled MonoVM assembly -> source-runtime-values.txt -> SourceLoader -> native ModPrefixes ID and ManagedPrefixBridge storage.
