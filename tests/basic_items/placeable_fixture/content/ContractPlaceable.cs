using BunnyLoader.Source;
public sealed class ContractPlaceable : BunnyItem
{
    public override void SetDefaults()
    {
        Texture="Content/Items/ContractPlaceable";
        DefaultToPlaceableTile("other/Workbench",3);
        Width=28;Height=14;Value=150;
    }
    public override void AddRecipes() => CreateRecipe().AddIngredient(36).AddIngredient("OtherMaterial",10).Register();
}
