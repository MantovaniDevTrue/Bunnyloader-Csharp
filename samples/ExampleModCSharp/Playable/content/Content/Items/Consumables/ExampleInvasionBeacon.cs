using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleInvasionBeacon : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Invasion Beacon";
        Tooltip = "Starts the Example Invasion during daytime at the surface";
        TooltipTranslations["pt-BR"] = "Inicia a invasão de exemplo durante o dia na superfície";
        Texture = "Content/Items/Consumables/ExampleInvasionBeacon.png";
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

    public override bool OnUse(int playerIndex) => false;

    public override bool OnUse(int playerIndex, BunnyUseContext context) =>
        context.CanStartSurfaceInvasion && ExampleInvasion.Start(true);

    public override string OnUseRejectionReason(int playerIndex, BunnyUseContext context) =>
        !context.Daytime ? "Nighttime" :
        !context.OnSurface ? "NotAtSurface" :
        !context.NoVanillaInvasion ? "VanillaInvasionActive" :
        !context.PlayerAlive ? "PlayerDead" :
        !context.WorldReady ? "WorldNotReady" :
        ExampleInvasion.Controller.Phase == BunnyInvasionPhase.Running ?
            "AlreadyRunning" : "StartRejected";

    public override void AddRecipes()
    {
        CreateRecipe()
            .AddIngredient<ExampleItem>(10)
            .AddTile(BunnyVanillaTiles.WorkBenches)
            .Register();
    }
}
