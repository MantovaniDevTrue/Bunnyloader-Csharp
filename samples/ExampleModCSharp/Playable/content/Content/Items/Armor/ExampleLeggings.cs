using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Armor;

public sealed class ExampleLeggings : BunnyItem
{
    public const int MoveSpeedBonus = 5;

    public override void SetDefaults()
    {
        Texture = "Content/Items/Armor/ExampleLeggings.png";
        DisplayName = "Example Leggings";
        Width = 18;
        Height = 18;
        Value = SellPrice(gold: 1);
        Rare = ItemRarityID.Green;
        Defense = 5;
        Tooltip = $"This is a modded leg armor.\n{MoveSpeedBonus}% increased movement speed";
        TooltipTranslations["pt-BR"] = $"Uma armadura de mod para as pernas.\n{MoveSpeedBonus}% de aumento na velocidade de movimento";
        Behavior.Armor.Add(BunnyArmorStat.MoveSpeed, MoveSpeedBonus / 100f);
        Behavior.EquipTexture(BunnyEquipKind.Legs, "Content/Items/Armor/ExampleLeggings_Legs");
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
    }
}
