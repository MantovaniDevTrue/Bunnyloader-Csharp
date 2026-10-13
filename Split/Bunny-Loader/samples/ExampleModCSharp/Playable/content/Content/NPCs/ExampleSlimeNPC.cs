// ExampleMod C# | Content/NPCs/ExampleSlimeNPC.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSlimeNPC : BunnyNPC
{
    public override void SetDefaults()
    {
        DisplayName = "Example Slime";
        Texture = "Content/NPCs/ExampleSlimeNPC.png";
        Width = 32;
        Height = 24;
        Damage = 7;
        Defense = 2;
        LifeMax = 25;
        AiStyle = 1;
        AnimationType = 1;
        Frames = 2;
    }
}

