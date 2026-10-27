// ExampleMod C# | Content/Items/Accessories/ExampleSwiftAccessory.cs
// Acessorio: Tooltip descreve o efeito; o BunnyPlayer vinculado decide o bonus ao equipar.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSwiftAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Swift Accessory";
        Texture = "Content/Items/Accessories/ExampleSwiftAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Aumenta a velocidade de corrida em 8%";
    }
}

