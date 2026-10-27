// ExampleMod C# | Common/Players/ExampleShadowPlayer.cs
// BunnyPlayer: confira quais callbacks sao de preparacao e quais recebem contexto durante o jogo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleShadowPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleShadowAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override bool FrameEffects(bool accessoryEquipped)
    {
        return accessoryEquipped;
    }
}

