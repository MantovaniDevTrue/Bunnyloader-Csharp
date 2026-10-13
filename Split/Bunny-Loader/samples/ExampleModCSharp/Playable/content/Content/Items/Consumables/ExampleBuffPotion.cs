// ExampleMod C# | Content/Items/Consumables/ExampleBuffPotion.cs
// Conteudo do exemplo. Leia os membros do SDK desta build antes de copiar uma API de outro loader.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBuffPotion : BunnyBuffPotion
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Potion";
        Texture = "Content/Items/Consumables/ExampleBuffPotion.png";
        Buff = nameof(ExampleDefenseBuff);
        Duration = 600;
    }
}

