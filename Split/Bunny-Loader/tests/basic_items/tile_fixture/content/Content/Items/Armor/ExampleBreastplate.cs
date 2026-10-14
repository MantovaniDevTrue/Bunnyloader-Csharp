using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Armor;

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
        Behavior.Armor.Add(BunnyArmorInteger.StatManaMax2, 20);
        Behavior.Armor.Add(BunnyArmorInteger.MaxMinions, 1);
        Behavior.Armor.ImmuneTo("OnFire");
        Behavior.EquipTexture(BunnyEquipKind.Body, "Content/Items/Armor/ExampleBreastplate_Body");
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
    }
}
