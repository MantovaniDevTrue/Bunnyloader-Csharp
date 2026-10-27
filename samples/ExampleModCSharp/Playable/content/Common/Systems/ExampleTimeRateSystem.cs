using BunnyLoader.Source;
using System;

namespace ExampleModCSharp;

/// <summary>
/// Exemplo do ModifyTimeRate portado para a API nativa do Bunny C#.
/// Desligado por padrão para preservar o tempo de mundos comuns.
/// </summary>
public sealed class ExampleTimeRateSystem : BunnyWorldUpdateLoader
{
    protected override bool EnableUpdates => false;

    protected override void ModifyTimeRate(ref BunnyTimeRateValues rates)
    {
        // Cada valor é ticks por atualização do mundo.
        // TimeRate altera a velocidade do relógio (dia/noite).
        // TileUpdateRate controla a velocidade de atualizações de tiles.
        // EventUpdateRate controla a taxa de progressão dos eventos.
        // O host conserva frações para não perder progresso ao fazer Floor.
        // Use taxas positivas e finitas; o host recusa NaN e infinito.
        rates.TimeRate = Math.Clamp(rates.TimeRate * 0.5, 0, 1000);
        rates.TileUpdateRate = Math.Clamp(rates.TileUpdateRate, 0, 1000);
        rates.EventUpdateRate = Math.Clamp(rates.EventUpdateRate * 0.5, 0, 1000);
    }

    protected override void UpdateUI(nint gameTime)
    {
        // GameTime é um ponteiro transitório do IL2CPP; nunca salve este
        // valor, nem tente liberá-lo com Marshal.FreeHGlobal.
        // O callback roda depois de UpdateUIStates e não roda no menu.
        // Desenho gráfico deve ser feito em um loader de UI apropriado.
    }
}
