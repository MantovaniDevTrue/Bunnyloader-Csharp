// ExampleMod C# | Common/Players/ExampleArcanePlayer.cs
// BunnyPlayer: confira quais callbacks sao de preparacao e quais recebem contexto durante o jogo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleArcanePlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleArcaneAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override int UpdateManaRegen(bool accessoryEquipped)
    {
        return accessoryEquipped ? 10 : 0;
    }
}

