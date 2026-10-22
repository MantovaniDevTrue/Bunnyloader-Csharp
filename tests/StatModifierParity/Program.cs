using BunnyLoader.Source;
using ExampleModCSharp.Recipes;

int passed = 0;
int failed = 0;

void Check(string name, Action test)
{
    try
    {
        test();
        passed++;
        Console.WriteLine("[PASS] " + name);
    }
    catch (Exception ex)
    {
        failed++;
        Console.Error.WriteLine("[FAIL] " + name + ": " + ex.Message);
    }
}

static void Near(float actual, float expected, string field)
{
    if (!float.IsFinite(actual) || !float.IsFinite(expected) ||
        MathF.Abs(actual - expected) > 0.0002f * MathF.Max(1f, MathF.Abs(expected)))
        throw new Exception(field + ": expected " + expected + ", got " + actual);
}

static void Verify(bool ok, string message)
{
    if (!ok) throw new Exception(message);
}

Check("Constructor identity", () =>
{
    StatModifier identity = new();
    Near(identity.Additive, 1f, "Additive");
    Near(identity.Multiplicative, 1f, "Multiplicative");
    Near(identity.Base, 0f, "Base");
    Near(identity.Flat, 0f, "Flat");
    Near(identity.ApplyTo(123f), 123f, "ApplyTo");
    Verify(identity == StatModifier.Default, "Default is not identity");
});

Check("default(struct) stays zero, matching tModLoader", () =>
{
    StatModifier uninitialized = default;
    Near(uninitialized.Additive, 0f, "Additive");
    Near(uninitialized.Multiplicative, 0f, "Multiplicative");
    Verify(uninitialized != StatModifier.Default, "default should not be identity");
});

Check("Explicit constructor and operation order", () =>
{
    StatModifier value = new(1.2f, 1.5f, 7f, 5f);
    Near(value.ApplyTo(100f), 196f, "ApplyTo");
    Near(value.Undo(196f), 100f, "Undo");
});

Check("Add subtract and both commutative operators", () =>
{
    StatModifier modifier = StatModifier.Default + 0.2f - 0.1f;
    Near(modifier.Additive, 1.1f, "Additive");
    Near((0.2f + modifier).Additive, 1.3f, "reverse addition");
    Near((2f * modifier).Multiplicative, 2f, "reverse multiplication");
});

Check("Multiplication and division preserve other components", () =>
{
    StatModifier modifier = new(1.3f, 1.5f, 7f, 5f);
    modifier *= 2f;
    modifier /= 3f;
    Near(modifier.Multiplicative, 1f, "Multiplicative");
    Near(modifier.Additive, 1.3f, "Additive");
    Near(modifier.Flat, 7f, "Flat");
    Near(modifier.Base, 5f, "Base");
});

Check("CombineWith is additive, not compound", () =>
{
    StatModifier left = new(1.2f, 1.5f, 3f, 2f);
    StatModifier right = new(1.1f, 2f, 4f, 1f);
    StatModifier result = left.CombineWith(right);
    Near(result.Additive, 1.3f, "Additive");
    Near(result.Multiplicative, 3f, "Multiplicative");
    Near(result.Flat, 7f, "Flat");
    Near(result.Base, 3f, "Base");
    Near(result.ApplyTo(10f), 57.7f, "ApplyTo");
});

Check("Scale centers multipliers around one", () =>
{
    StatModifier modifier = new(1.2f, 1.5f, 3f, 2f).Scale(0.5f);
    Near(modifier.Additive, 1.1f, "Additive");
    Near(modifier.Multiplicative, 1.25f, "Multiplicative");
    Near(modifier.Flat, 1.5f, "Flat");
    Near(modifier.Base, 1f, "Base");
});

Check("Scale identity and zero", () =>
{
    StatModifier modifier = new(1.2f, 1.5f, 3f, 2f);
    Verify(modifier.Scale(1f) == modifier, "Scale 1 must keep all fields");
    Verify(modifier.Scale(0f) == StatModifier.Default, "Scale 0 must produce identity");
});

Check("Equality and GetHashCode", () =>
{
    StatModifier a = new(1.2f, 1.5f, 3f, 2f);
    StatModifier b = new(1.2f, 1.5f, 3f, 2f);
    Verify(a.Equals((object)b) && a == b && !(a != b), "Equivalent modifiers differ");
    Verify(a.GetHashCode() == b.GetHashCode(), "Equal modifiers have different hashes");
    b.Flat++;
    Verify(a != b && !a.Equals((object)b), "Flat is missing from equality");
});

Check("StatInheritanceData Full and None", () =>
{
    StatInheritanceData full = StatInheritanceData.Full;
    StatInheritanceData none = StatInheritanceData.None;
    Near(full.damageInheritance, 1f, "Full damage");
    Near(full.critChanceInheritance, 1f, "Full crit");
    Near(full.attackSpeedInheritance, 1f, "Full speed");
    Near(full.armorPenInheritance, 1f, "Full armor pen");
    Near(full.knockbackInheritance, 1f, "Full knockback");
    Near(none.damageInheritance + none.critChanceInheritance +
        none.attackSpeedInheritance + none.armorPenInheritance + none.knockbackInheritance,
        0f, "None");
});

Check("Partial inheritance preserves other zero fields", () =>
{
    StatInheritanceData halfCrit = new(damageInheritance: 1f, critChanceInheritance: 0.5f);
    Near(halfCrit.damageInheritance, 1f, "damage");
    Near(halfCrit.critChanceInheritance, 0.5f, "crit");
    Near(halfCrit.attackSpeedInheritance, 0f, "speed");
    Near(halfCrit.armorPenInheritance, 0f, "pen");
    Near(halfCrit.knockbackInheritance, 0f, "knockback");
});

Check("ExampleMod C# recipe really compiles and computes", () =>
{
    Near(ExampleStatModifierRecipe.CalculateDamage(100f), 128.664f, "sample damage");
    Near(ExampleStatModifierRecipe.CalculateInheritedDamage(100f), 130f, "sample inherited damage");
});

Console.WriteLine($"StatModifier parity: {passed} passed, {failed} failed.");
return failed == 0 ? 0 : 1;
