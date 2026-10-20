using BunnyLoader.Source;

namespace ExampleModCSharp.Tiles;

// Equivalente ao ExampleTile.js original. O mesmo chão também será utilizado
// pela ExampleTree; colocar um bloco não significa implementar a árvore.
public sealed class ExampleTile : BunnyTile {
    public override void SetStaticDefaults() {
        Texture = "Content/Tiles/ExampleTile.png";
        DisplayName = "Example Block";
        // A colisão e os frames continuam sendo calculados pelo Terraria.
        // BlockLight bloqueia luz, enquanto MergeDirt/ChecksForMerge permitem
        // que o motor escolha os frames de junção com a terra vizinha.
        Flags = BunnyTileFlags.Solid | BunnyTileFlags.BlockLight |
            BunnyTileFlags.MergeDirt | BunnyTileFlags.ChecksForMerge;
        DustName = "Stone";
        AddMapEntry(0, 200, 255, "ExampleTile");
    }
}
