using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Mounts;

public sealed class ExampleMountItem : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName="Example Mount Item";
        Texture="Content/Items/Mounts/ExampleMountItem.png";
        Width=20;
        Height=30;
        UseTime=20;
        UseAnimation=20;
        UseStyle=1;
        Value=SellPrice(gold:3);
        Rare=ItemRarityID.Green;
        UseSound="Item79";
        NoMelee=true;
        Mount="ExampleMount";
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
    }
}
