using BunnyLoader.Source;

namespace ExampleItems.Furniture;

public sealed class ExampleWorkbench : BunnyItem {
    public override void SetDefaults() {
        Texture = "Content/Items/Placeable/Furniture/ExampleWorkbench.png";
        DisplayName = "Example Workbench";
        Tooltip = "Counts as a Work Bench";
        TooltipTranslations["pt-BR"] = "Conta como bancada nas receitas";
        DefaultToPlaceableTile("ExampleWorkbench");
        Width = 28;
        Height = 14;
        Value = 150;
    }

    public override void AddRecipes() {
        CreateRecipe()
            .AddIngredient(36)
            .AddIngredient("ExampleItem", 10)
            .Register();

        // Receita adicional da API Bunny: o Terraria aceita qualquer madeira do
        // grupo nativo, inclusive variantes, e consome dez Example Items.
        // RequireGroup e instalado pelo nucleo; nenhum JavaScript e executado.
        var woodRecipe = CreateRecipe()
            .AddNativeRecipeGroup("Wood", 10)
            .AddIngredient<ExampleModCSharp.ExampleItem>(10);
        woodRecipe.Register();

        // Clone tambem funciona depois de Register: a copia nao compartilha a
        // lista de ingredientes. Ferro OU chumbo substitui a madeira somente nela.
        var metalRecipe = woodRecipe.Clone();
        metalRecipe.RemoveNativeRecipeGroup("Wood");
        metalRecipe.AddNativeRecipeGroup("IronBar", 2).Register();
    }
}
