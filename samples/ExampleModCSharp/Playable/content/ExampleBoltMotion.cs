// ExampleMod C# | ExampleBoltMotion.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using System;

namespace ExampleModCSharp;

public static class ExampleBoltMotion
{
    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.25f) * 0.025f;
    }
}

