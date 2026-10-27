// ExampleMod C# | Common/GlobalItems/ExampleWeaponTuningGlobalItem.cs
// GlobalItem com filtro por estatística (como os do tModLoader que usam DamageType):
// toda espada corpo a corpo de madeira ganha +10% de dano e +4 de crítico.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWeaponTuningGlobalItem : BunnyGlobalItem
{
    private const int WoodenSword = 24; // ItemID.WoodenSword

    public override bool AppliesToEntity(BunnyVanillaItem item) =>
        item.Type == WoodenSword && item.DamageType == DamageClass.Melee;

    public override void ModifyWeaponDamage(BunnyVanillaItem item, ref StatModifier damage) => damage += 0.10f;

    public override void ModifyWeaponCrit(BunnyVanillaItem item, ref float crit) => crit += 4f;
}
