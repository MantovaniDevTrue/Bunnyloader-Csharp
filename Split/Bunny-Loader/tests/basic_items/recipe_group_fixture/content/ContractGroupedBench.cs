using BunnyLoader.Source;

public sealed class ContractGroupedBench : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Contract Grouped Bench";
        Texture = "Contract/Bench.png";
        MaxStack = 9999;
    }

    public override void AddRecipes()
    {
        CreateRecipe()
            .AddNativeRecipeGroup("Wood", 10)
            .AddNativeRecipeGroup("IronBar", 2)
            .AddIngredient<ContractGroupedBench>(3)
            .AddTile("Bench")
            .Register();
    }
}
