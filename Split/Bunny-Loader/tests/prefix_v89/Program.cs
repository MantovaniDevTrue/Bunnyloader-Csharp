using BunnyLoader.SourceCompiler;
using System.Globalization;

if (args.Length != 1 || !File.Exists(args[0]))
    throw new InvalidOperationException("Expected compiled C# prefix fixture path");

string temp = Path.Combine(Path.GetTempPath(), "bunny-prefix-v89-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
try
{
    string diagnostics = Path.Combine(temp, "diagnostics.txt");
    File.WriteAllText(diagnostics, "");
    int classes = CompiledAssemblyProbe.Verify(args[0], diagnostics);
    if (classes != 5)
        throw new InvalidOperationException("Expected two content types and three managed prefixes, got " + classes);

    var data = File.ReadAllLines(Path.Combine(temp, "source-runtime-values.txt"));
    if (data.Length != 21 || data[0] != "BUNNY_MANAGED_DEFAULTS_V2")
        throw new InvalidOperationException("Unexpected MonoVM native handoff");

    if (data.Count(row => row == "PREFIX_DENY\tTestWand\t71,TestMightyPrefix") != 1)
        throw new InvalidOperationException("Item-owned prefix denials missing from native wire");
    if (data.Count(row => row == "PREFIX_DENY_STACK\tTestWand\t72:3:5,TestCustomPrefix:4:6") != 1)
        throw new InvalidOperationException("Dynamic item prefix stack denials missing");
    if (data.Count(row => row == "PREFIX_DENY_ATTRIBUTE\tTestWand\t73:D:10:25,TestCustomPrefix:M:2:8") != 1)
        throw new InvalidOperationException("Dynamic item damage/mana denials missing");
    if (data.Count(row => row == "PREFIX_CHOOSE_STACK\tTestWand\tTestCustomPrefix:1:2") != 1)
        throw new InvalidOperationException("Native item ChoosePrefix policy missing");
    if (data.Count(row => row == "PREFIX_CHANCE_STACK\tTestWand\t3:5:1") != 1)
        throw new InvalidOperationException("Managed per-instance PrefixChance missing");
    if (data.Count(row => row == "PREFIX_CHANCE\tTestWand\t0") != 1)
        throw new InvalidOperationException("Managed item PrefixChance policy missing");
    if (data.Count(row => row == "PREFIX_APPLY\tTestWand\t4\t125") != 1)
        throw new InvalidOperationException("Managed item ApplyPrefix policy missing");
    if (data.Count(row => row == "PREFIX_CONDITION\tTestMightyPrefix\t10\t25\t0\t8\t1\t10") != 1)
        throw new InvalidOperationException("Per-instance item condition descriptor missing");
    if (data.Count(row => row == "PREFIX_STACK_WEIGHT\tTestMightyPrefix\t3\t5\t6") != 1)
        throw new InvalidOperationException("Dynamic per-instance roll weight descriptor missing");
    if (data.Count(row => row == "PREFIX_CALLBACK\tTestMightyPrefix\t8") != 1)
        throw new InvalidOperationException("Managed prefix Apply callback mask missing");
    if (data.Count(row => row == "PREFIX_CALLBACK\tTestGuardedPrefix\t23") != 1)
        throw new InvalidOperationException("Managed prefix override mask missing");
    if (data.Count(row => row == "PREFIX_ATTRIBUTE_WEIGHT\tTestGuardedPrefix\tM\t0\t8\t4") != 1)
        throw new InvalidOperationException("Per-instance mana RollChance descriptor missing");
    if (data.Count(row => row == "PREFIX_POST\tTestMightyPrefix\t3\t40\t0\tBonus adicional do prefixo") != 1)
        throw new InvalidOperationException("Managed BunnyPrefix.Apply, tooltip and stat efficacy missing");
    string tipsHex = string.Join(",",
        Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("First line")),
        Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("Second line")));
    if (data.Count(row => row == "PREFIX_TOOLTIP\tTestMightyPrefix\t" + tipsHex) != 1)
        throw new InvalidOperationException("Multiple prefix tooltip lines missing");
    var dynamicTipHex = Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes(
        "Dano {damage}, mana {mana}, pilha {stack}"));
    if (data.Count(row => row == "PREFIX_TOOLTIP_ITEM\tTestMightyPrefix\t" + dynamicTipHex) != 1)
        throw new InvalidOperationException("Current item tooltip template missing");
    var byName = data.Skip(1).Where(row => !row.StartsWith("PREFIX_TOOLTIP\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_TOOLTIP_ITEM\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_DENY\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_DENY_STACK\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_DENY_ATTRIBUTE\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_CONDITION\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_STACK_WEIGHT\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_ATTRIBUTE_WEIGHT\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_CALLBACK\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_POST\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_CHANCE\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_CHANCE_STACK\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_CHOOSE_STACK\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_APPLY\t", StringComparison.Ordinal))
        .Select(row => row.Split('\t'))
        .ToDictionary(fields => fields[1], StringComparer.Ordinal);
    if (byName.Count != 5 || !byName.ContainsKey("TestBolt") || !byName.ContainsKey("TestWand"))
        throw new InvalidOperationException("Source content lost in prefix handoff");

    void Check(string name, int category, float weight, float damage, float value, int defense)
    {
        string[] fields = byName[name];
        int expectedLength = name == "TestMightyPrefix" || name == "TestCustomPrefix" ? 17 : 16;
        if (fields.Length != expectedLength || fields[0] != "PREFIX" ||
            int.Parse(fields[2], CultureInfo.InvariantCulture) != category ||
            MathF.Abs(float.Parse(fields[3], CultureInfo.InvariantCulture) - weight) > .0001f ||
            MathF.Abs(float.Parse(fields[4], CultureInfo.InvariantCulture) - damage) > .0001f ||
            MathF.Abs(float.Parse(fields[13], CultureInfo.InvariantCulture) - value) > .0001f ||
            int.Parse(fields[14], CultureInfo.InvariantCulture) != defense ||
            string.IsNullOrWhiteSpace(fields[15]))
            throw new InvalidOperationException("Managed prefix does not match expected descriptor: " + name);
    }

    Check("TestMightyPrefix", 4, 5f, 1.2f, 1.26f, 0);
    if (byName["TestMightyPrefix"][16] != "1:3:0,2:0:1,512:7,TestWand:2")
        throw new InvalidOperationException("Managed prefix per-item rules are not canonical");
    Check("TestGuardedPrefix", 5, 1f, 1f, 1.2f, 4);
    Check("TestCustomPrefix", 6, 1f, 1f, 1f, 0);
    if (byName["TestCustomPrefix"][16] != "1:0,TestWand:0:1:1")
        throw new InvalidOperationException("Custom prefix must keep explicit item rules only");
    var rules = new BunnyLoader.Source.BunnyPrefixRollRules();
    rules.Deny(12);
    rules.Allow(12, 5);
    if (rules.Serialize() != "12:5")
        throw new InvalidOperationException("Managed prefix item rule replace failed");
    bool invalidRejected = false;
    try { rules.SetWeight(0, 1); }
    catch (ArgumentOutOfRangeException) { invalidRejected = true; }
    if (!invalidRejected)
        throw new InvalidOperationException("Invalid item type accepted");
    bool prematureRejected = false;
    try { rules.SetCanRoll(18, true); }
    catch (InvalidOperationException) { prematureRejected = true; }
    if (!prematureRejected)
        throw new InvalidOperationException("CanRoll override without item rule accepted");
    bool preferenceRejected = false;
    try { rules.PreferFor(12); }
    catch (InvalidOperationException) { preferenceRejected = true; }
    if (preferenceRejected)
        throw new InvalidOperationException("Positive weight should allow a preferred prefix");
    if (rules.Serialize() != "12:5:1:1")
        throw new InvalidOperationException("Preferred prefix wire missing");
    rules.Deny(12);
    if (rules.Serialize() != "12:0")
        throw new InvalidOperationException("Deny must cancel preferred prefix");
    bool disallowedPreferenceRejected = false;
    try { rules.PreferFor(12); }
    catch (InvalidOperationException) { disallowedPreferenceRejected = true; }
    if (!disallowedPreferenceRejected)
        throw new InvalidOperationException("Disallowed preferred item accepted");
    var named = new BunnyLoader.Source.BunnyPrefixRollRules();
    named.Deny("ExampleWand");
    named.Allow("ExampleWand", 6);
    named.SetCanRoll("ExampleWand", false);
    if (named.Serialize() != "ExampleWand:6:0")
        throw new InvalidOperationException("Named item eligibility override failed");
    named.Deny("ExampleWand");
    if (named.Serialize() != "ExampleWand:0")
        throw new InvalidOperationException("Deny must clear explicit eligibility");
    bool invalidNameRejected = false;
    try { named.Deny("examplemod/ExampleWand"); }
    catch (ArgumentOutOfRangeException) { invalidNameRejected = true; }
    if (!invalidNameRejected)
        throw new InvalidOperationException("Unsafe stable item name accepted");
    var predicates = new BunnyLoader.Source.BunnyPrefixItemConditions();
    bool invalidDamage = false;
    try { predicates.RequireDamageBetween(30, 10); }
    catch (ArgumentOutOfRangeException) { invalidDamage = true; }
    if (!invalidDamage) throw new InvalidOperationException("Inverted damage range accepted");
    bool invalidMana = false;
    try { predicates.RequireManaBetween(-1, 4); }
    catch (ArgumentOutOfRangeException) { invalidMana = true; }
    if (!invalidMana) throw new InvalidOperationException("Negative mana range accepted");
    predicates.RequireDamageBetween(10, 25);
    predicates.RequireManaBetween(0, 8);
    bool invalidStack = false;
    try { predicates.RequireStackBetween(11, 10); }
    catch (ArgumentOutOfRangeException) { invalidStack = true; }
    if (!invalidStack) throw new InvalidOperationException("Inverted stack range accepted");
    predicates.RequireStackBetween(1, 10);
    bool invalidWeight = false;
    try { predicates.OverrideRollWeightWhenStackBetween(5, 3, 6); }
    catch (ArgumentOutOfRangeException) { invalidWeight = true; }
    if (!invalidWeight) throw new InvalidOperationException("Inverted stack weight range accepted");
    predicates.OverrideRollWeightWhenStackBetween(3, 5, 6);
    bool duplicateWeight = false;
    try { predicates.OverrideRollWeightWhenStackBetween(3, 5, 2); }
    catch (InvalidOperationException) { duplicateWeight = true; }
    if (!duplicateWeight) throw new InvalidOperationException("Duplicate stack weight policy accepted");
    bool conflictingDynamicWeight = false;
    try { predicates.OverrideRollWeightWhenManaBetween(0, 8, 4); }
    catch (InvalidOperationException) { conflictingDynamicWeight = true; }
    if (!conflictingDynamicWeight) throw new InvalidOperationException("Conflicting stack/attribute dynamic weight accepted");
    var attributeWeight = new BunnyLoader.Source.BunnyPrefixItemConditions();
    bool invalidAttributeWeight = false;
    try { attributeWeight.OverrideRollWeightWhenDamageBetween(25, 10, 4); }
    catch (ArgumentOutOfRangeException) { invalidAttributeWeight = true; }
    if (!invalidAttributeWeight) throw new InvalidOperationException("Inverted damage weight interval accepted");
    invalidAttributeWeight = false;
    try { attributeWeight.OverrideRollWeightWhenManaBetween(0, 10001, 4); }
    catch (ArgumentOutOfRangeException) { invalidAttributeWeight = true; }
    if (!invalidAttributeWeight) throw new InvalidOperationException("Excessive mana weight interval accepted");
    attributeWeight.OverrideRollWeightWhenDamageBetween(10, 25, 0);
    if (!attributeWeight.HasAttributeRollWeight ||
        attributeWeight.AttributeRollWeightKind != 'D' ||
        attributeWeight.AttributeRollWeightMinimum != 10 ||
        attributeWeight.AttributeRollWeightMaximum != 25 ||
        attributeWeight.AttributeRollWeightValue != 0)
        throw new InvalidOperationException("Damage weight state not retained");
    conflictingDynamicWeight = false;
    try { attributeWeight.OverrideRollWeightWhenStackBetween(1, 10, 6); }
    catch (InvalidOperationException) { conflictingDynamicWeight = true; }
    if (!conflictingDynamicWeight) throw new InvalidOperationException("Conflicting attribute/stack weight accepted");
    if (!predicates.HasStackRollWeight || predicates.RollWeightMinStack != 3 ||
        predicates.RollWeightMaxStack != 5 || predicates.RollWeightInRange != 6)
        throw new InvalidOperationException("Dynamic stack weight was not kept");
    if (predicates.MinDamage != 10 || predicates.MaxDamage != 25 ||
        predicates.MinMana != 0 || predicates.MaxMana != 8 ||
        predicates.MinStack != 1 || predicates.MaxStack != 10)
        throw new InvalidOperationException("Item condition assignment lost");
    var choose = new BunnyLoader.Source.BunnyItemPrefixRules();
    if (choose.SerializeStackChoice() != "")
        throw new InvalidOperationException("Unset item prefix choice must not emit a row");
    bool invalidChoice = false;
    try { choose.ChooseWhenStackBetween(0, 1, 2); }
    catch (ArgumentOutOfRangeException) { invalidChoice = true; }
    if (!invalidChoice) throw new InvalidOperationException("Invalid chosen prefix ID accepted");
    invalidChoice = false;
    try { choose.ChooseWhenStackBetween("external/ForeignPrefix", 1, 2); }
    catch (ArgumentOutOfRangeException) { invalidChoice = true; }
    if (!invalidChoice) throw new InvalidOperationException("External chosen prefix accepted");
    invalidChoice = false;
    try { choose.ChooseWhenStackBetween(71, 3, 2); }
    catch (ArgumentOutOfRangeException) { invalidChoice = true; }
    if (!invalidChoice) throw new InvalidOperationException("Inverted prefix choice interval accepted");
    choose.ChooseWhenStackBetween("TestCustomPrefix", 1, 2);
    if (choose.SerializeStackChoice() != "TestCustomPrefix:1:2")
        throw new InvalidOperationException("Native item prefix choice serialization changed");
    bool repeatedChoice = false;
    try { choose.ChooseWhenStackBetween(71, 1, 2); }
    catch (InvalidOperationException) { repeatedChoice = true; }
    if (!repeatedChoice) throw new InvalidOperationException("Duplicate prefix choice accepted");
    var denied = new BunnyLoader.Source.BunnyItemPrefixRules();
    denied.Deny("TestMightyPrefix");
    denied.Deny(71);
    if (denied.Serialize() != "71,TestMightyPrefix")
        throw new InvalidOperationException("Prefix denial serialization is not canonical");
    bool badPrefix = false;
    try { denied.Deny(0); }
    catch (ArgumentOutOfRangeException) { badPrefix = true; }
    if (!badPrefix) throw new InvalidOperationException("Invalid vanilla prefix accepted");
    bool badName = false;
    try { denied.Deny("external/OtherPrefix"); }
    catch (ArgumentOutOfRangeException) { badName = true; }
    if (!badName) throw new InvalidOperationException("External prefix lookup accepted");
    denied.DenyWhenStackBetween(72, 3, 5);
    denied.DenyWhenStackBetween("TestCustomPrefix", 4, 6);
    if (denied.SerializeStackDenials() != "72:3:5,TestCustomPrefix:4:6")
        throw new InvalidOperationException("Conditional prefix deny serialization is not canonical");
    bool duplicateDynamic = false;
    try { denied.DenyWhenStackBetween(72, 3, 5); }
    catch (InvalidOperationException) { duplicateDynamic = true; }
    if (!duplicateDynamic) throw new InvalidOperationException("Duplicate conditional deny accepted");
    bool invalidDynamic = false;
    try { denied.DenyWhenStackBetween(73, 6, 5); }
    catch (ArgumentOutOfRangeException) { invalidDynamic = true; }
    if (!invalidDynamic) throw new InvalidOperationException("Invalid conditional deny range accepted");
    denied.DenyWhenDamageBetween(73, 10, 25);
    denied.DenyWhenManaBetween("TestCustomPrefix", 2, 8);
    if (denied.SerializeAttributeDenials() != "73:D:10:25,TestCustomPrefix:M:2:8")
        throw new InvalidOperationException("Damage/mana prefix deny serialization changed");
    bool invalidAttribute = false;
    try { denied.DenyWhenManaBetween(74, 0, 10001); }
    catch (ArgumentOutOfRangeException) { invalidAttribute = true; }
    if (!invalidAttribute) throw new InvalidOperationException("Invalid mana bounds accepted");
    bool duplicateAttribute = false;
    try { denied.DenyWhenDamageBetween(73, 12, 24); }
    catch (InvalidOperationException) { duplicateAttribute = true; }
    if (!duplicateAttribute) throw new InvalidOperationException("Duplicate damage condition accepted");
    bool externalAttribute = false;
    try { denied.DenyWhenDamageBetween("foreign/Other", 10, 20); }
    catch (ArgumentOutOfRangeException) { externalAttribute = true; }
    if (!externalAttribute) throw new InvalidOperationException("External prefix accepted as local");
    if (denied.SerializeStackChance() != "")
        throw new InvalidOperationException("Unset conditional PrefixChance must be empty");
    bool badChanceRange = false;
    try { denied.SetPrefixChanceWhenStackBetween(6, 5, true); }
    catch (ArgumentOutOfRangeException) { badChanceRange = true; }
    if (!badChanceRange) throw new InvalidOperationException("Invalid chance range accepted");
    denied.SetPrefixChanceWhenStackBetween(3, 5, false);
    if (denied.SerializeStackChance() != "3:5:0")
        throw new InvalidOperationException("Conditional false PrefixChance serialization failed");
    bool repeatedChance = false;
    try { denied.SetPrefixChanceWhenStackBetween(3, 5, true); }
    catch (InvalidOperationException) { repeatedChance = true; }
    if (!repeatedChance) throw new InvalidOperationException("Duplicate PrefixChance policy accepted");
    string prefixWarmup = BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 2, "");
    if (prefixWarmup != "READY")
        throw new InvalidOperationException("MonoVM prefix callback warmup rejected: " + prefixWarmup);
    string validItem = "6207,22,4,1,98,1";
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 0, validItem) != "1")
        throw new InvalidOperationException("Managed prefix CanRoll item snapshot rejected");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 1, validItem) != "4")
        throw new InvalidOperationException("Managed prefix RollChance item snapshot rejected");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 0, "6207,10,4,1,98,1") != "0")
        throw new InvalidOperationException("Managed prefix CanRoll damage rejection failed");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 1, "6207,22,9,1,98,1") != "0")
        throw new InvalidOperationException("Managed prefix RollChance mana rejection failed");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 1, "6207,-1,4,1,98,1") != "4")
        throw new InvalidOperationException("Damage -1 accessory snapshot was rejected");
    if (!BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 0, "6207,-1,4,1,98,0")
        .StartsWith("ERROR|"))
        throw new InvalidOperationException("Damage -1 non-accessory was accepted");
    foreach (string bad in new[] { "6207,22,4,1,98", "6207,22,4,1,98,8",
                                  "6207,-2,4,1,98,1", "6207,22,4,1,256,1" })
        if (!BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
            "prefix-fixture", args[0], "TestGuardedPrefix", 0, 0, bad).StartsWith("ERROR|"))
            throw new InvalidOperationException("Malformed managed prefix item snapshot accepted");
    if (!BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "MissingPrefix", 0, 2, "").StartsWith("ERROR|"))
        throw new InvalidOperationException("Unknown managed prefix callback owner accepted");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 3, validItem) != "1")
        throw new InvalidOperationException("Managed prefix stat efficacy callback failed");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 3, "6207,26,4,1,98,1") != "0")
        throw new InvalidOperationException("Managed prefix stat efficacy rejection failed");
    string tooltipText = "0:" + Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("Dano 22")) +
        ";1:" + Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("Ruim"));
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 5, validItem) != tooltipText)
        throw new InvalidOperationException("Dynamic managed tooltip lines and negative flags missing");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestMightyPrefix", 0, 2, "") != "READY")
        throw new InvalidOperationException("Managed prefix Apply warmup failed");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestMightyPrefix", 0, 4, validItem) != "2,10")
        throw new InvalidOperationException("Managed prefix dynamic Apply outcome failed");
    if (BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestMightyPrefix", 0, 4, "6207,18,4,1,98,1") != "0,0")
        throw new InvalidOperationException("Managed prefix dynamic Apply low-damage outcome failed");
    if (!BunnyLoader.Source.BunnyPrefixRuntime.Dispatch(
        "prefix-fixture", args[0], "TestGuardedPrefix", 0, 5, validItem).StartsWith("ERROR|"))
        throw new InvalidOperationException("Unsupported prefix callback kind accepted");
    Console.WriteLine("BUNNY_CSHARP_PREFIX_NATIVE_CONTRACT=PASS prefixes=3 callbacks=5 content=2 managed_worker=ready");
}
finally
{
    Directory.Delete(temp, true);
}
