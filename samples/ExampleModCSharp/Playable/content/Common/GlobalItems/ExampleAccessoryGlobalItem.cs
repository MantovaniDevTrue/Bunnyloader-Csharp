// ExampleMod C# | Common/GlobalItems/ExampleAccessoryGlobalItem.cs
// UpdateEquip em item vanilla: as Garras Selvagens dão +5% de dano corpo a corpo
// enquanto equipadas, pelo mesmo canal do AddClassBonusOnEquip dos itens C#.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleAccessoryGlobalItem : BunnyGlobalItem
{
    private const int FeralClaws = 211; // ItemID.FeralClaws

    public override bool AppliesToEntity(BunnyVanillaItem item) => item.Type == FeralClaws;

    public override void UpdateEquip(BunnyVanillaItem item, BunnyEquipEffects effects)
    {
        effects.AddClassBonus(new BunnyClassBonus(DamageClass.Melee) { Damage = 0.05f });
    }
}
