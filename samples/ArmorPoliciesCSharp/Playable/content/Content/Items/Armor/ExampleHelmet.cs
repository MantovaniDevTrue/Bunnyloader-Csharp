using BunnyLoader.Source;

namespace ArmorPoliciesCSharp;

public sealed class ExampleHelmet : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/Armor/ExampleHelmet.png";
        DisplayName = "Example Helmet";
        Width = 18;
        Height = 18;
        Value = 10000;
        Rare = 2;
        Defense = 5;
        Tooltip = "This is a modded helmet.";
        TooltipTranslations["pt-BR"] = "Um capacete de mod.";
        var set = new BunnyArmorSet("ExampleHelmet", "ExampleBreastplate",
            "ExampleLeggings", "20% increased damage");
        set.Effects.Add(BunnyArmorStat.MeleeDamage, 0.2f);
        set.Effects.Add(BunnyArmorStat.RangedDamage, 0.2f);
        set.Effects.Add(BunnyArmorStat.MagicDamage, 0.2f);
        set.Effects.Add(BunnyArmorStat.MinionDamage, 0.2f);
        Behavior.DefineArmorSet(set);
        Behavior.EquipTexture(BunnyEquipKind.Head, "Content/Items/Armor/ExampleHelmet_Head");
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(2, 2).AddTile(18).Register();
    }
}
