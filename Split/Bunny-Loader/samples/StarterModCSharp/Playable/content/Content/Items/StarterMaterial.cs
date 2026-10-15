using BunnyLoader.Source;

namespace StarterModCSharp;

public sealed class StarterMaterial : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Starter Material";
        Texture = "Content/Items/StarterMaterial.png";
        MaxStack = 9999;
        Value = 100;
        Tooltip = "One material, one native recipe.";
        TooltipTranslations["pt-BR"] = "Um material, uma receita nativa.";
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient(2, 1).Register();
    }
}
