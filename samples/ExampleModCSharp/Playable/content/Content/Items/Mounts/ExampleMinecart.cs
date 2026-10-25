using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Mounts;

public sealed class ExampleMinecart : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName="Example Minecart";
        Texture="Content/Items/Mounts/ExampleMinecart.png";
        Width=34;
        Height=22;
        Value=SellPrice(gold:1);
        Rare=ItemRarityID.Blue;
        Mount="ExampleMinecartMount";
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(2343).AddIngredient("ExampleItem",15).AddTile(16).Register();
    }
}
