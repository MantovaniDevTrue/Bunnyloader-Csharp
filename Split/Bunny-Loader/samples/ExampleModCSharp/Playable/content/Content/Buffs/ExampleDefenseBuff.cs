// ExampleMod C# | Content/Buffs/ExampleDefenseBuff.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleDefenseBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense";
        Texture = "Content/Buffs/ExampleDefenseBuff.png";
        DefenseBonus = 10;
    }
}

