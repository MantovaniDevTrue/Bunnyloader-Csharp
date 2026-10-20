using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class WaspNest : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Wasp Nest";
        Texture = "Content/Items/Accessories/WaspNest";
        Tooltip = "Suas abelhas ficam mais fortes";
        Width = 26; Height = 26; Accessory = true;
        Value = 100000; Rare = 1; // sellPrice de duas moedas de ouro.
        Behavior.EquipTexture(BunnyEquipKind.Back, "Content/Items/Accessories/WaspNest_Back");
        // ApplyEquipFunctional define strongBees; vaidade nunca concede o efeito.
        Behavior.SetOnEquip(BunnyEquipFlag.StrongBees);
    }
}
