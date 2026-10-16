using System;
using BunnyLoader.CSharp.Experimental.Prefix;

static void Near(float actual, float expected, string label)
{
    if (MathF.Abs(actual - expected) > 0.00001f)
        throw new Exception($"{label}: esperado {expected}, recebido {actual}");
}

static void CheckWeapon(PrefixDraft prefix, float damage, float price, int power)
{
    var stats = PrefixStatsDraft.Identity;
    float value = 1f;
    var item = new PrefixItemDraft(true, false);
    if (prefix.Category != PrefixCategoryDraft.AnyWeapon || !prefix.CanRoll(item) ||
        prefix.RollChance(item) != 5f)
        throw new Exception("Contrato de rolagem de prefixo incorreto");
    prefix.SetStats(ref stats);
    prefix.ModifyValue(ref value);
    Near(stats.DamageMultiplier, damage, "dano " + power);
    Near(value, price, "preco " + power);
    Near(stats.ShootSpeedMultiplier, 1f, "velocidade sem alteracao");
    Near(stats.ManaMultiplier, 1f, "mana sem alteracao");
    Near(stats.UseTimeMultiplier, 1f, "tempo sem alteracao");
}

CheckWeapon(new ExampleWeaponPrefixDraft(), 1.2f, 1.05f, 1);
CheckWeapon(new ExampleDerivedPrefixDraft(), 1.4f, 1.1f, 2);

var accessory = new ExampleAccessoryPrefixDraft();
var accessoryItem = new PrefixItemDraft(false, true);
if (accessory.Category != PrefixCategoryDraft.Accessory || !accessory.CanRoll(accessoryItem))
    throw new Exception("Categoria de acessorio incorreta");
int defense = 10;
float accessoryPrice = 1f;
accessory.ApplyAccessoryEffects(ref defense);
accessory.ModifyValue(ref accessoryPrice);
if (defense != 14) throw new Exception("Bonus de defesa incorreto");
Near(accessoryPrice, 1.2f, "preco de acessorio");

var disabled = new ZeroChancePrefix();
if (disabled.CanRoll(new PrefixItemDraft(true, false)))
    throw new Exception("Prefixo com peso zero nao pode rolar");

Console.WriteLine("BUNNY_CSHARP_PREFIX_DRAFT_CONTRACT=PASS cases=4");

sealed class ZeroChancePrefix : PrefixDraft
{
    public override float RollChance(PrefixItemDraft item) => 0f;
}
