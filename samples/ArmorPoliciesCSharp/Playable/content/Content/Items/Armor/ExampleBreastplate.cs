using BunnyLoader.Source;

namespace ArmorPoliciesCSharp;

public sealed class ExampleBreastplate : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/Armor/ExampleBreastplate.png";
        DisplayName = "Example Breastplate";
        Width = 18;
        Height = 18;
        Value = 10000;
        Rare = 2;
        Defense = 6;
        Tooltip = "This is a modded body armor.\nImmunity to 'On Fire!'\n+20 max mana and +1 max minions";
        TooltipTranslations["pt-BR"] = "Uma armadura de mod para o corpo.\nImunidade a 'Em Chamas!'\n+20 de mana máxima e +1 de lacaio máximo";
        Behavior.Armor.Add(BunnyArmorInteger.StatManaMax2, 20);
        Behavior.Armor.Add(BunnyArmorInteger.MaxMinions, 1);
        Behavior.Armor.ImmuneTo("OnFire");
        Behavior.EquipTexture(BunnyEquipKind.Body, "Content/Items/Armor/ExampleBreastplate_Body");
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(2, 2).AddTile(18).Register();
    }
}
