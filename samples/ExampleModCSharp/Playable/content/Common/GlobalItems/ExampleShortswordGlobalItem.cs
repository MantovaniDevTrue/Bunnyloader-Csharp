// ExampleMod C# | Common/GlobalItems/ExampleShortswordGlobalItem.cs
// Porte do ShortswordGlobalItem do ExampleMod do tModLoader: muda um item vanilla.
// O Bunny avalia AppliesToEntity/SetDefaults uma vez sobre a foto dos itens vanilla
// e aplica o resultado no SetDefaults nativo do jogo.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleShortswordGlobalItem : BunnyGlobalItem
{
    private const int CopperShortsword = 3507; // ItemID.CopperShortsword

    public override bool AppliesToEntity(BunnyVanillaItem item) => item.Type == CopperShortsword;

    public override void SetDefaults(BunnyVanillaItem item)
    {
        item.Damage = 50; // Como no tModLoader: dano 50.
        // O Shoot (granadas) do original depende do momento do uso e fica para
        // os callbacks de uso do GlobalItem.
    }
}
