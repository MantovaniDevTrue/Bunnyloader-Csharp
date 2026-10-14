using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBeard : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Beard";
        Texture = "Content/Items/Accessories/ExampleBeard";
        Width = 18; Height = 14; MaxStack = 1; Value = 50000;
        Accessory = true; Vanity = true;
        // A textura usa o slot Beard do Terraria e acompanha a animacao nativa.
        Behavior.EquipTexture(BunnyEquipKind.Beard, "Content/Items/Accessories/ExampleBeard_Beard", BunnyEquipDrawFlags.UseHairColor);
        Behavior.ColorFromPlayerHair = true;
    }
}
