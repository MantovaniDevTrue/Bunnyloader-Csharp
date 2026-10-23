using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBuffPotion : BunnyBuffPotion
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Potion";
        Texture = "Content/Items/Consumables/ExampleBuffPotion.png";
        Buff = nameof(ExampleDefenseBuff);
        Duration = 5400;
        NativeDefaults.SetInt("width", 18)
            .SetInt("height", 26)
            .SetId("useStyle", "ItemUseStyleID", "DrinkLiquid")
            .SetInt("useAnimation", 15)
            .SetInt("useTime", 15)
            .SetBool("useTurn", true)
            .SetInt("maxStack", 9999)
            .SetBool("consumable", true)
            .SetId("rare", "ItemRarityID", "Orange")
            .SetInt("value", 50000)
            .SetVanillaUseSound("Item3")
            .SetItemDrinkParticleColors(new BunnyColor(240, 240, 240),
                new BunnyColor(200, 200, 200),new BunnyColor(140, 140, 140));
    }
}
