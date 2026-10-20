using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleInvasionBeacon : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Invasion Beacon";
        Tooltip = "Starts the Example Invasion";
        TooltipTranslations["pt-BR"] = "Inicia a invasão de exemplo";
        Texture = "Content/Items/ExampleItem.png";
        Width = 24;
        Height = 28;
        MaxStack = 1;
        Value = BuyPrice(silver: 50);
        Rare = 1;
        UseStyle = 4;
        UseTime = 30;
        UseAnimation = 30;
        Consumable = false;
    }

    public override bool OnUse(int playerIndex) =>
        ExampleInvasion.Start(true);

    public override void AddRecipes()
    {
        CreateRecipe()
            .AddIngredient<ExampleItem>(10)
            .AddTile(BunnyVanillaTiles.WorkBenches)
            .Register();
    }
}
