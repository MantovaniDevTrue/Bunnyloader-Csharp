using BunnyLoader.Source;

namespace NativeAccessoryCSharp;

public sealed class NativeCharm : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Native Charm";
        Texture = "Content/Items/NativeCharm.png";
        Tooltip = "+10% melee damage, +5% movement speed and stronger bees";
        TooltipTranslations["pt-BR"] = "+10% de dano corpo a corpo, +5% de velocidade e abelhas mais fortes";
        Width = 28;
        Height = 28;
        Accessory = true;
        Value = 100;
        Rare = 1;
        Defense = 2;
        // Operations run in declaration order after the functional equip hook.
        Behavior.MultiplyOnEquip(BunnyEquipFloat.MeleeDamage, 1.1f);
        Behavior.AddOnEquip(BunnyEquipFloat.MoveSpeed, 0.05f);
        Behavior.SetOnEquip(BunnyEquipFlag.StrongBees);
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(2, 2).Register(); // Dirt Block, no station.
    }
}
