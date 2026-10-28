using Terraria;
using Terraria.ID;
using Terraria.ModLoader;

namespace ExampleMod.Content.Items.Weapons.Melee;

// A espada do ExampleMod. Os numeros sao os do original.
public class ExampleMeleeWeapon : ModItem
{
    public override void SetDefaults()
    {
        Item.width = 40;
        Item.height = 40;

        Item.useStyle = ItemUseStyleID.Swing;
        Item.useTime = 20;
        Item.useAnimation = 20;
        Item.autoReuse = true;

        Item.melee = true;
        // (dano, repulsao, critico)
        Item.SetWeaponValues(50, 6, 6);

        Item.value = Item.sellPrice(gold: 1);
        Item.rare = ItemRarityID.Green;
        Item.UseSound = SoundID.Item1;
    }
}
