using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleStatAccessory : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Stat Accessory";
        Texture = "Content/Items/Accessories/ExampleStatAccessory";
        Tooltip = "Aumenta em 10% o dano corpo a corpo, a distancia e magico";
        Width = 28; Height = 28; Accessory = true; Value = 250000; Rare = ItemRarityID.Green;
        // Multiplicativo, como o original: nao e +10 pontos nem afeta invocacoes.
        Behavior.MultiplyOnEquip(BunnyEquipFloat.MeleeDamage, 1.1f);
        Behavior.MultiplyOnEquip(BunnyEquipFloat.RangedDamage, 1.1f);
        Behavior.MultiplyOnEquip(BunnyEquipFloat.MagicDamage, 1.1f);
    }
}
