using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleCostume : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Costume";
        Texture = "Content/Items/Armor/ExampleCostume";
        Width = 24;
        Height = 28;
        Accessory = true;
        MaxStack = 1;
        Value = BuyPrice(gold: 15);
        Rare = ItemRarityID.Pink;
        NativeDefaults.SetBool("hasVanityEffects", true);
        Behavior.EquipTexture(BunnyEquipKind.Head, "Content/Items/Armor/ExampleCostume_Head");
        Behavior.EquipTexture(BunnyEquipKind.Body, "Content/Items/Armor/ExampleCostume_Body");
        Behavior.EquipTexture(BunnyEquipKind.Legs, "Content/Items/Armor/ExampleCostume_Legs");
        Behavior.ConfigureCostume("Content/Items/Armor/ExampleCostumeAlt", "Blocky", 4.8f, 45);
    }
}
