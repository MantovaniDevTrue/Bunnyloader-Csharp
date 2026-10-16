// ExampleMod C# | Common/Players/ExampleFocusPlayer.cs
// BunnyPlayer: confira quais callbacks sao de preparacao e quais recebem contexto durante o jogo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleFocusPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleFocusAccessory);
    }

    public override void ResetEffects()
    {
        DefenseBonus = 0;
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            DefenseBonus = 7;
    }
}

