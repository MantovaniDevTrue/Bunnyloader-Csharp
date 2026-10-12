using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBuffPotion : BunnyBuffPotion
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Potion";
        Texture = "Content/Items/Consumables/ExampleBuffPotion.png";
        Buff = nameof(ExampleDefenseBuff);
        Duration = 600;
    }
}
