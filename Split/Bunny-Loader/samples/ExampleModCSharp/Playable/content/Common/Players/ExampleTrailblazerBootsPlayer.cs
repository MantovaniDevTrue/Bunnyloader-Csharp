using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleTrailblazerBootsPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleTrailblazerBootsAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            MoveSpeedPercent = 12;
    }

    public override int UpdateMovement(bool accessoryEquipped)
    {
        return accessoryEquipped ? 8 : 0;
    }

    public override bool FrameEffects(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            BunnyVisual.SetEquipmentSprite(0, 1002,
                "68c3115f-62f4-49f2-a56b-388256781d5f:Content/Items/Accessories/ExampleTrailblazerBootsAccessory_Shoes.png");
        return accessoryEquipped;
    }
}
