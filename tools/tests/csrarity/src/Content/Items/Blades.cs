using CsRarity.Content.Rarities;
using Terraria;
using Terraria.ID;
using Terraria.ModLoader;

namespace CsRarity.Content.Items;

public abstract class Blade : ModItem
{
    protected abstract int Rarity { get; }

    public override void SetDefaults()
    {
        Item.width = 40;
        Item.height = 40;
        Item.useStyle = ItemUseStyleID.Swing;
        Item.useTime = 20;
        Item.useAnimation = 20;
        Item.melee = true;
        Item.SetWeaponValues(20, 5, 4);
        Item.value = Item.sellPrice(silver: 50);
        Item.rare = Rarity;
    }
}

public class PlainBlade : Blade
{
    protected override int Rarity => ModContent.RarityType<Plain>();
}

public class ShinyBlade : Blade
{
    protected override int Rarity => ModContent.RarityType<Shiny>();
}
