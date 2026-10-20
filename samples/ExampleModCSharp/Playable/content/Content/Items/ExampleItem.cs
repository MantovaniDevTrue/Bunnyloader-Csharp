using BunnyLoader.Source;

namespace ExampleModCSharp;

// Material do ExampleModBunny: mesmo valor/pilha e mesma receita de 999 unidades.
// A moeda customizada ExampleCustomCurrency do arquivo JS ainda esta no backlog
// de lojas; este arquivo demonstra o ITEM, nao finge registrar uma moeda.
public sealed class ExampleItem : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Item";
        Tooltip = "Item description";
        TooltipTranslations["pt-BR"] = "Descrição do item";
        Texture = "Content/Items/ExampleItem.png";
        MaxStack = 9999; // ModItem.CommonMaxStack, como no original JS.
        Value = 100;    // buyPrice(0,0,1,0): valor interno em moedas de cobre.
        SetRarity<ExampleModRarity>();
    }

    public override void AddRecipes()
    {
        // A receita e instalada pelo host na thread do Terraria. So confirma
        // sucesso depois de registrar e atualizar os lookups de materiais/crafting.
        CreateRecipe(999)
            .AddIngredient(BunnyVanillaItems.DirtBlock, 10)
            .AddTile(BunnyVanillaTiles.WorkBenches)
            .Register();
    }
}
