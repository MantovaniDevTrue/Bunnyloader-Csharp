// ExampleMod C# | Content/Items/Accessories/ExampleMomentumAccessory.cs
// Acessorio: Tooltip descreve o efeito; o BunnyPlayer vinculado decide o bonus ao equipar.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMomentumAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Momentum Accessory";
        Texture = "Content/Items/Accessories/ExampleMomentumAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Aumenta a aceleracao ao correr em 6%";
    }
}

