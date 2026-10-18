using BunnyLoader.Source;
namespace ExampleModCSharp;

// SDK demonstration: use, observe the 25% defense reduction, then remove the
// debuff through the normal Terraria UI/nurse rules. Original buffs remain intact.
public sealed class ExampleDebuffPotion : BunnyBuffPotion
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Debuff Potion";
        Texture = "Content/Items/Consumables/ExampleDebuffPotion.png";
        Buff = nameof(ExampleDefenseDebuff);
        Duration = 600;
    }
}
