using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Armor;

public sealed class ExampleBreastplate : BunnyItem
{
    public const int MaxManaIncrease = 20;
    public const int MaxMinionIncrease = 1;

    public override void SetDefaults()
    {
        Texture = "Content/Items/Armor/ExampleBreastplate.png";
        DisplayName = "Example Breastplate";
        Width = 18;
        Height = 18;
        Value = SellPrice(gold: 1);
        Rare = ItemRarityID.Green;
        Defense = 6;
        // As mesmas constantes substituem os placeholders do tooltip original
        // e alimentam UpdateEquip, evitando descrição e efeito divergentes.
        Tooltip = $"This is a modded body armor.\nImmunity to 'On Fire!'\n+{MaxManaIncrease} max mana and +{MaxMinionIncrease} max minions";
        TooltipTranslations["pt-BR"] = $"Uma armadura de mod para o corpo.\nImunidade a 'Em Chamas!'\n+{MaxManaIncrease} de mana máxima e +{MaxMinionIncrease} de lacaio máximo";
        Behavior.Armor.Add(BunnyArmorInteger.StatManaMax2, MaxManaIncrease);
        Behavior.Armor.Add(BunnyArmorInteger.MaxMinions, MaxMinionIncrease);
        Behavior.Armor.ImmuneTo("OnFire");
        Behavior.EquipTexture(BunnyEquipKind.Body, "Content/Items/Armor/ExampleBreastplate_Body");
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
    }
}
