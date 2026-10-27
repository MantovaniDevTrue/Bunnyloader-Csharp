// ExampleMod C# | Content/Projectiles/ExampleWaveBoltMotion.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using System;

namespace ExampleModCSharp;

public static class ExampleWaveBoltMotion
{
    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.20f) * 0.018f;
    }
}

