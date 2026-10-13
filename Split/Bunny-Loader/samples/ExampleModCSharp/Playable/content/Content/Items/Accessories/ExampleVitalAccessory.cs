// ExampleMod C# | Content/Items/Accessories/ExampleVitalAccessory.cs
// Acessorio: Tooltip descreve o efeito; o BunnyPlayer vinculado decide o bonus ao equipar.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleVitalAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Vital Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
        Tooltip = "Regenera +12 vida; reducao adicional contra NPC e projetil; esquiva no quarto golpe";
    }
}

