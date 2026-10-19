using BunnyLoader.Source;

namespace ExampleModCSharp.Items.Placeable;

public sealed class ExampleTileItem : BunnyItem {
    public override void SetDefaults() {
        Texture = "Content/Items/Placeable/ExampleTileItem.png";
        DisplayName = "Example Block";
        // Nome local, resolvido no registro nativo deste mod a cada sessão.
        // Não salve o ID numérico: ele pode mudar com a ordem dos mods.
        DefaultToPlaceableTile("ExampleTile");
        Width = 12;
        Height = 12;
    }

    public override void AddRecipes() {
        // Receita original: um ExampleItem produz dez blocos. A bancada
        // vanilla 18 é uma condição da receita original, não o objeto de teste.
        CreateRecipe(10).AddIngredient<ExampleModCSharp.ExampleItem>()
            .AddTile(18).Register();
    }
}
