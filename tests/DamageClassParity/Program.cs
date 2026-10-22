using BunnyLoader.Source;
using ExampleModCSharp.DamageClasses;
using System.Reflection;
using System.Reflection.Emit;

int passed = 0;
int failed = 0;

void Check(string name, Action action)
{
    try { action(); passed++; Console.WriteLine("[PASS] " + name); }
    catch (Exception ex) { failed++; Console.Error.WriteLine("[FAIL] " + name + ": " + ex); }
}
static void Verify(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}
static void Near(float actual, float expected, string field)
{
    if (!float.IsFinite(actual) ||
        MathF.Abs(actual - expected) > 0.0002f * MathF.Max(1f, MathF.Abs(expected)))
        throw new InvalidOperationException(field + ": expected " + expected + ", got " + actual);
}
static void Throws<T>(Action action) where T : Exception
{
    try { action(); }
    catch (T) { return; }
    throw new InvalidOperationException("Expected " + typeof(T).Name);
}

Check("Ten vanilla DamageClass IDs match tModLoader order", () =>
{
    DamageClass[] vanilla = { DamageClass.Default, DamageClass.Generic, DamageClass.Melee,
        DamageClass.MeleeNoSpeed, DamageClass.Ranged, DamageClass.Magic, DamageClass.Summon,
        DamageClass.SummonMeleeSpeed, DamageClass.MagicSummonHybrid, DamageClass.Throwing };
    Verify(DamageClassLoader.DamageClassCount == vanilla.Length, "count");
    for (int i = 0; i < vanilla.Length; i++)
        Verify(vanilla[i].Type == i && ReferenceEquals(DamageClassLoader.GetDamageClass(i), vanilla[i]), "id " + i);
    Verify(DamageClassLoader.GetDamageClass(-1) is null &&
        DamageClassLoader.GetDamageClass(999) is null, "unknown ID");
});

Check("Self-effect always true; Generic is not an effect parent", () =>
{
    Verify(DamageClass.Melee.CountsAsClass(DamageClass.Melee), "self");
    Verify(!DamageClass.Melee.CountsAsClass(DamageClass.Generic), "Generic effect");
    Verify(!DamageClass.Default.CountsAsClass(DamageClass.Generic), "Default");
});

Check("MeleeNoSpeed inherits melee effect and prefixes", () =>
{
    Verify(DamageClass.MeleeNoSpeed.CountsAsClass(DamageClass.Melee), "melee effect");
    Verify(DamageClass.MeleeNoSpeed.GetsPrefixesFor(DamageClass.Melee), "prefixes");
    Verify(!DamageClass.MeleeNoSpeed.CountsAsClass(DamageClass.Magic), "magic effect");
});

Check("Summon prevents standard crit and tooltip lines", () =>
{
    var player = new DamageClassPlayer();
    Verify(!DamageClass.Summon.UseStandardCritCalcs, "summon crit");
    Verify(!DamageClass.Summon.ShowStatTooltipLine(player, "CritChance"), "crit tooltip");
    Verify(!DamageClass.Summon.ShowStatTooltipLine(player, "Speed"), "speed tooltip");
    Verify(DamageClass.Summon.ShowStatTooltipLine(player, "Damage"), "damage tooltip");
});

Check("SummonMeleeSpeed uses different effect and prefix inheritance", () =>
{
    var cls = DamageClass.SummonMeleeSpeed;
    Verify(cls.CountsAsClass(DamageClass.Summon), "summon effects");
    Verify(!cls.CountsAsClass(DamageClass.Melee), "not melee effects");
    Verify(cls.GetsPrefixesFor(DamageClass.Melee), "melee prefix");
    Verify(!cls.GetsPrefixesFor(DamageClass.Summon), "summon prefix");
});

Check("MagicSummonHybrid effects and Throwing prefixes match tModLoader", () =>
{
    var cls = DamageClass.MagicSummonHybrid;
    Verify(cls.CountsAsClass(DamageClass.Magic) && cls.CountsAsClass(DamageClass.Summon), "hybrid effects");
    Verify(!cls.CountsAsClass(DamageClass.Melee), "hybrid melee");
    Verify(DamageClass.Throwing.GetsPrefixesFor(DamageClass.Ranged), "throwing ranged prefix");
    Verify(!DamageClass.Throwing.CountsAsClass(DamageClass.Ranged), "throwing ranged effect");
});

Check("Generic starts with four crit; Default inherits none", () =>
{
    var p = new DamageClassPlayer();
    Near(p.GetCritChance(DamageClass.Generic), 4f, "generic default");
    Near(p.GetTotalCritChance(DamageClass.Melee), 4f, "melee inherits");
    Near(p.GetTotalCritChance(DamageClass.Default), 0f, "default excludes");
});

Check("Generic damage bonuses flow only to non-Default classes", () =>
{
    var p = new DamageClassPlayer();
    p.GetDamage(DamageClass.Generic) += 0.25f;
    Near(p.GetTotalDamage(DamageClass.Melee).ApplyTo(100f), 125f, "melee");
    Near(p.GetTotalDamage(DamageClass.Default).ApplyTo(100f), 100f, "default");
});

Check("MeleeNoSpeed inherits Generic and Melee damage but not their speed", () =>
{
    var p = new DamageClassPlayer();
    p.GetDamage(DamageClass.Generic) += 0.20f;
    p.GetDamage(DamageClass.Melee) += 0.30f;
    p.GetAttackSpeed(DamageClass.Generic) += 0.15f;
    p.GetAttackSpeed(DamageClass.Melee) += 0.25f;
    Near(p.GetTotalDamage(DamageClass.MeleeNoSpeed).ApplyTo(100f), 150f, "damage");
    Near(p.GetTotalAttackSpeed(DamageClass.MeleeNoSpeed), 1f, "speed");
    Near(p.GetTotalAttackSpeed(DamageClass.Melee), 1.4f, "melee speed");
});

Check("SummonMeleeSpeed inherits Melee speed only", () =>
{
    var p = new DamageClassPlayer();
    p.GetDamage(DamageClass.Melee) += 0.50f;
    p.GetDamage(DamageClass.Summon) += 0.20f;
    p.GetAttackSpeed(DamageClass.Melee) += 0.25f;
    p.GetAttackSpeed(DamageClass.Summon) += 0.15f;
    Near(p.GetTotalDamage(DamageClass.SummonMeleeSpeed).ApplyTo(100f), 120f, "no melee damage");
    Near(p.GetTotalAttackSpeed(DamageClass.SummonMeleeSpeed), 1.4f, "summon plus melee speed");
});

Check("MagicSummonHybrid sums all three damage classes additively", () =>
{
    var p = new DamageClassPlayer();
    p.GetDamage(DamageClass.Generic) += 0.10f;
    p.GetDamage(DamageClass.Magic) += 0.20f;
    p.GetDamage(DamageClass.Summon) += 0.30f;
    Near(p.GetTotalDamage(DamageClass.MagicSummonHybrid).ApplyTo(100f), 160f, "hybrid");
});

Check("Damage and knockback use separate multiplicative factors", () =>
{
    var p = new DamageClassPlayer();
    p.GetKnockback(DamageClass.Generic) *= 1.5f;
    p.GetKnockback(DamageClass.Melee) *= 2f;
    Near(p.GetTotalKnockback(DamageClass.Melee).ApplyTo(10f), 30f, "knockback");
    Near(p.GetTotalDamage(DamageClass.Melee).ApplyTo(10f), 10f, "unchanged damage");
});

Check("Get ref slots update the same stat storage", () =>
{
    var p = new DamageClassPlayer();
    ref StatModifier damage = ref p.GetDamage(DamageClass.Magic);
    damage += 0.20f;
    p.GetArmorPenetration(DamageClass.Magic) += 5f;
    Near(p.GetDamage(DamageClass.Magic).Additive, 1.2f, "ref damage");
    Near(p.GetTotalArmorPenetration(DamageClass.Magic), 5f, "ref pen");
});

Check("Vanilla class generic GetInstance and generic GetTotal", () =>
{
    var p = new DamageClassPlayer();
    Verify(ReferenceEquals(DamageClassLoader.GetInstance<MeleeDamageClass>(), DamageClass.Melee), "instance");
    p.GetDamage<MeleeDamageClass>() += 0.50f;
    Near(p.GetTotalDamage<MeleeDamageClass>().ApplyTo(100f), 150f, "generic total");
});

var example = new ExampleDamageClass();
Check("Register ExampleMod custom class once; preserve ten vanilla IDs", () =>
{
    int type = DamageClassLoader.Register(example);
    Verify(type == 10 && example.Type == 10, "custom id");
    Verify(ReferenceEquals(DamageClassLoader.GetInstance<ExampleDamageClass>(), example), "typed instance");
    Verify(DamageClass.Melee.Type == 2, "vanilla id changed");
    Throws<InvalidOperationException>(() => DamageClassLoader.Register(example));
    Throws<InvalidOperationException>(() => DamageClassLoader.Register(new ExampleDamageClass()));
});

Check("Custom effects are melee and magic but stats only inherit Generic", () =>
{
    Verify(example.CountsAsClass(DamageClass.Melee), "melee effects");
    Verify(example.CountsAsClass(DamageClass.Magic), "magic effects");
    Verify(!example.CountsAsClass(DamageClass.Summon), "no summon effects");
    var p = new DamageClassPlayer();
    p.GetDamage(DamageClass.Generic) += 0.20f;
    p.GetDamage(DamageClass.Melee) += 0.30f;
    p.GetDamage(DamageClass.Magic) += 0.50f;
    Near(p.GetTotalDamage(example).ApplyTo(100f), 120f, "only Generic damage");
});

Check("Custom class default crit and armor pen plus inherited generic", () =>
{
    var p = new DamageClassPlayer();
    Near(p.GetTotalCritChance(example), 8f, "4 own plus 4 Generic");
    Near(p.GetTotalArmorPenetration(example), 10f, "own pen");
    Verify(!example.ShowStatTooltipLine(p, "Speed"), "hide Speed");
    Verify(example.ShowStatTooltipLine(p, "Damage"), "show Damage");
});

Check("Reset effects clears bonuses and restores class defaults", () =>
{
    var p = new DamageClassPlayer();
    p.GetCritChance(example) += 10f;
    p.GetDamage(example) += 1f;
    p.ResetEffects();
    Near(p.GetTotalCritChance(example), 8f, "reset crit");
    Near(p.GetTotalDamage(example).ApplyTo(100f), 100f, "reset damage");
});

var half = new HalfRangedDamageClass();
Check("Partial, negative and amplified inheritance are not clamped", () =>
{
    DamageClassLoader.Register(half);
    var p = new DamageClassPlayer();
    p.GetDamage(DamageClass.Ranged) += 0.20f;
    p.GetCritChance(DamageClass.Ranged) += 10f;
    p.GetAttackSpeed(DamageClass.Ranged) += 0.20f;
    p.GetKnockback(DamageClass.Ranged) += 0.40f;
    Near(p.GetTotalDamage(half).ApplyTo(100f), 110f, "half damage");
    Near(p.GetTotalCritChance(half), -5f, "negative crit");
    Near(p.GetTotalAttackSpeed(half), 1.4f, "amplified speed");
    Near(p.GetTotalKnockback(half).ApplyTo(100f), 120f, "half knockback");
});

Check("Invalid IDs, type lookups and vanilla re-registration fail safely", () =>
{
    Verify(DamageClassLoader.GetDamageClass(-10) is null, "negative ID");
    Throws<InvalidOperationException>(() => DamageClassLoader.GetInstance<UnregisteredDamageClass>());
    Throws<ArgumentException>(() => DamageClassLoader.Register(new RangedDamageClass()));
});

Check("Failed effect-cache rebuild rolls back registration atomically", () =>
{
    int count = DamageClassLoader.DamageClassCount;
    var broken = new ExplodingDamageClass();
    Throws<InvalidOperationException>(() => DamageClassLoader.Register(broken));
    Verify(count == DamageClassLoader.DamageClassCount && broken.Type == -1, "rollback");
});

Check("Unload clears only custom classes and invalidates stale player snapshots", () =>
{
    var p = new DamageClassPlayer();
    int oldGeneration = DamageClassLoader.Generation;
    DamageClassLoader.Unload();
    Verify(DamageClassLoader.DamageClassCount == 10, "vanilla count");
    Verify(DamageClassLoader.Generation != oldGeneration, "generation");
    Verify(example.Type == -1 && half.Type == -1, "old type");
    Throws<InvalidOperationException>(() => { _ = p.GetTotalDamage(example); });
    Near(p.GetTotalCritChance(DamageClass.Melee), 4f, "fresh default");
});

// Dois assemblies separados: descarregar o primeiro não pode renumerar o segundo.
static Assembly DynamicMod(string modName, string className)
{
    var builder = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName(modName),
        AssemblyBuilderAccess.Run);
    var module = builder.DefineDynamicModule(modName);
    TypeBuilder type = module.DefineType(className, TypeAttributes.Public |
        TypeAttributes.Class, typeof(DamageClass));
    type.DefineDefaultConstructor(MethodAttributes.Public);
    type.CreateType();
    return builder;
}

Assembly modA = DynamicMod("DamageParityModA", "DamageParityClassA");
Assembly modB = DynamicMod("DamageParityModB", "DamageParityClassB");
DamageClass? classA = null, classB = null;
Check("Two independent C# assemblies register with stable distinct IDs", () =>
{
    Verify(DamageClassLoader.RegisterAssembly("mod-a", modA) == 1, "register A");
    Verify(DamageClassLoader.RegisterAssembly("mod-b", modB) == 1, "register B");
    classA = DamageClassLoader.GetDamageClass(10);
    classB = DamageClassLoader.GetDamageClass(11);
    Verify(classA != null && classB != null && classA.Type == 10 && classB.Type == 11, "ids");
    Verify(DamageClassLoader.All.Count == 12, "two active classes");
});
Check("Unregister only first mod; second keeps original ID and effects", () =>
{
    var p = new DamageClassPlayer();
    Verify(DamageClassLoader.UnregisterMod("mod-a"), "unregister A");
    Verify(DamageClassLoader.GetDamageClass(10) is null, "A tombstone");
    Verify(ReferenceEquals(DamageClassLoader.GetDamageClass(11), classB), "B unchanged");
    Verify(classA!.Type == -1, "A invalid");
    Verify(DamageClassLoader.All.Count == 11, "active snapshot skips tombstone");
    Verify(classB!.CountsAsClass(classB), "B self-effect");
    Near(p.GetTotalCritChance(classB), 4f, "B inherited generic crit");
    Throws<InvalidOperationException>(() => { _ = p.GetTotalDamage(classA); });
});
Check("Re-register another mod never reuses retired class ID", () =>
{
    Verify(DamageClassLoader.RegisterAssembly("mod-a", DynamicMod("DamageParityModA2", "DamageParityClassA2")) == 1,
        "register new A");
    DamageClass? newA = DamageClassLoader.GetDamageClass(12);
    Verify(newA != null && newA.Type == 12, "new ID");
    Verify(DamageClassLoader.GetDamageClass(10) is null, "old ID stays vacant");
    Verify(ReferenceEquals(DamageClassLoader.GetDamageClass(11), classB), "B stable");
});
Check("Per-mod unload is idempotent and global unload restores vanilla", () =>
{
    Verify(DamageClassLoader.UnregisterMod("mod-a"), "new A removed");
    Verify(!DamageClassLoader.UnregisterMod("mod-a"), "double unload");
    Verify(!DamageClassLoader.UnregisterMod("missing-mod"), "unknown mod");
    Verify(DamageClassLoader.UnregisterMod("mod-b"), "B removed");
    Verify(DamageClassLoader.GetDamageClass(11) is null, "B tombstone");
    Verify(DamageClassLoader.All.Count == 10, "only vanilla active");
    DamageClassLoader.Unload();
    Verify(DamageClassLoader.DamageClassCount == 10 && DamageClassLoader.All.Count == 10,
        "global reset releases tombstones");
});

Console.WriteLine($"DamageClass parity: {passed} passed, {failed} failed.");
return failed == 0 ? 0 : 1;

sealed class HalfRangedDamageClass : DamageClass
{
    public override StatInheritanceData GetModifierInheritance(DamageClass other) =>
        ReferenceEquals(other, Ranged) ? new StatInheritanceData(.5f, -.5f, 2f, 0f, .5f) :
        StatInheritanceData.None;
}
sealed class UnregisteredDamageClass : DamageClass { }
sealed class ExplodingDamageClass : DamageClass
{
    public override bool GetEffectInheritance(DamageClass other) =>
        ReferenceEquals(other, Melee) ? throw new InvalidOperationException("test cache rollback") : false;
}
