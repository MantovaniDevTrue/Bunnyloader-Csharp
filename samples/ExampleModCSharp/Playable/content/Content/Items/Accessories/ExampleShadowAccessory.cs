// ExampleMod C# | Content/Items/Accessories/ExampleShadowAccessory.cs
// Acessorio: Tooltip descreve o efeito; o BunnyPlayer vinculado decide o bonus ao equipar.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleShadowAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Shadow Accessory";
        Texture = "Content/Items/Accessories/ExampleShadowAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Ativa um efeito visual de sombra enquanto equipado";
    }
}

