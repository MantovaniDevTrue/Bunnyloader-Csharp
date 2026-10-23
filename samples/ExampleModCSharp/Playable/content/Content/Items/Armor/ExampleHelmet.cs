using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Armor;

public sealed class ExampleHelmet : BunnyItem
{
    public const int AdditiveGenericDamageBonus = 20;

    public override void SetDefaults()
    {
        Texture = "Content/Items/Armor/ExampleHelmet.png";
        DisplayName = "Example Helmet";
        Width = 18;
        Height = 18;
        // Mesmo sellPrice(0, 1, 0, 0) do Bunny JS: 50.000 no valor
        // interno; buyPrice(gold: 1) produziria 10.000 e mudaria a venda.
        Value = SellPrice(gold: 1);
        Rare = ItemRarityID.Green;
        Defense = 5;
        Tooltip = "This is a modded helmet.";
        TooltipTranslations["pt-BR"] = "Um capacete de mod.";
        // O loader nativo aplica o conjunto apenas com as três peças. Os 20%
        // são aditivos nas quatro classes de dano, como no UpdateArmorSet JS.
        var set = new BunnyArmorSet(nameof(ExampleHelmet), nameof(ExampleBreastplate),
            nameof(ExampleLeggings), $"Increases dealt damage by {AdditiveGenericDamageBonus}%");
        set.Effects.Add(BunnyArmorStat.MeleeDamage, AdditiveGenericDamageBonus / 100f);
        set.Effects.Add(BunnyArmorStat.RangedDamage, AdditiveGenericDamageBonus / 100f);
        set.Effects.Add(BunnyArmorStat.MagicDamage, AdditiveGenericDamageBonus / 100f);
        set.Effects.Add(BunnyArmorStat.MinionDamage, AdditiveGenericDamageBonus / 100f);
        Behavior.DefineArmorSet(set);
        Behavior.EquipTexture(BunnyEquipKind.Head, "Content/Items/Armor/ExampleHelmet_Head");
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
    }
}
