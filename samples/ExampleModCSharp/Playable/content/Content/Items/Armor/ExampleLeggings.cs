using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Armor;

public sealed class ExampleLeggings : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/Armor/ExampleLeggings.png";
        DisplayName = "Example Leggings";
        Width = 18;
        Height = 18;
        Value = 10000;
        Rare = 2;
        Defense = 5;
        Tooltip = "This is a modded leg armor.\n5% increased movement speed";
        TooltipTranslations["pt-BR"] = "Uma armadura de mod para as pernas.\n5% de aumento na velocidade de movimento";
        Behavior.Armor.Add(BunnyArmorStat.MoveSpeed, 0.05f);
        Behavior.EquipTexture(BunnyEquipKind.Legs, "Content/Items/Armor/ExampleLeggings_Legs");
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
    }
}
