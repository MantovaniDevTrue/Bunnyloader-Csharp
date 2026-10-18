using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleRobe : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Robe";
        Texture = "Content/Items/Armor/Vanity/ExampleRobe";
        Width = 18; Height = 14; Rare = 1; Vanity = true;
        Behavior.EquipTexture(BunnyEquipKind.Body, "Content/Items/Armor/Vanity/ExampleRobe_Body", BunnyEquipDrawFlags.ShowHands);
        Behavior.EquipTexture(BunnyEquipKind.Legs, "Content/Items/Armor/Vanity/ExampleRobe_Legs");
        // SetMatch troca as pernas pelo manto. Equipar calcas nao corta a saia.
        Behavior.Robe = true;
    }
}
