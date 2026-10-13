// ExampleMod C# | Common/Players/ExampleSwiftPlayer.cs
// BunnyPlayer: confira quais callbacks sao de preparacao e quais recebem contexto durante o jogo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSwiftPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleSwiftAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            MoveSpeedPercent = 8;
    }
}

