using BunnyLoader.Source;

namespace NativePlacementProbe;

public sealed class ProbeWorkbench : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Teste C# Bancada";
        Texture = "Content/Items/ProbeWorkbench.png";
        // O nucleo escreve createTile/placeStyle no Item real, sem chamar QuickJS.
        // Tile 18 e a bancada vanilla; tile 19 e a plataforma vanilla.
        DefaultToPlaceableTile(18, 0);
        // A chamada acima configura uso, animacao, consumo e MaxStack=9999.
        // Podemos ajustar outras propriedades depois, como em qualquer item C#.
        Width = 28;
        Height = 14;
        Value = 0;
        Tooltip = "Teste de colocacao nativa. Quebrar devolve o item vanilla.";
    }

    public override void AddRecipes()
    {
        // Uma terra (ItemID 2) fabrica dez unidades, sem precisar de estacao.
        CreateRecipe(10).AddIngredient(2).Register();
    }
}
