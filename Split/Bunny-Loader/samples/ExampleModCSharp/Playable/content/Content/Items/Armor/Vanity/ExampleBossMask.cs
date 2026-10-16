using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBossMask : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Boss Mask";
        Texture = "Content/Items/Armor/Vanity/ExampleBossMask";
        Width = 22; Height = 28; Rare = 1; Value = 37500; Vanity = true; MaxStack = 1;
        Behavior.EquipTexture(BunnyEquipKind.Head, "Content/Items/Armor/Vanity/ExampleBossMask_Head");
    }
}
