using BunnyLoader.Source;

namespace ExampleModCSharp.Items.Placeable;

public sealed class ExampleWallAdvanced : BunnyItem {
    public override void SetDefaults() {
        Texture = "Content/Items/Placeable/ExampleWallAdvanced.png";
        DisplayName = "Example Advanced Wall";
        // A chave local é resolvida no registro compartilhado a cada sessão.
        // O helper real do Android define tempo de uso, pilha e colocação.
        // Atribuições feitas depois desta chamada podem alterar esses defaults.
        DefaultToPlaceableWall("ExampleWallAdvanced");
    }
    public override void AddRecipes() {
        // Receita original: 1 ExampleItem na bancada C# produz 4 paredes.
        CreateRecipe(4).AddIngredient<ExampleModCSharp.ExampleItem>()
            .AddTile("ExampleWorkbench").Register();
    }
}
