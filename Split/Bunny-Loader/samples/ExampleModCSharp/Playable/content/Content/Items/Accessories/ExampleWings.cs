using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Items.Accessories;

public sealed class ExampleWings : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/Accessories/ExampleWings.png";
        DisplayName = "Example Wings";
        Width = 22;
        Height = 20;
        Value = 10000;
        Rare = 2;
        Accessory = true;
        // O slot nativo usa a folha de asas do original; o Terraria anima o voo.
        Behavior.EquipTexture(BunnyEquipKind.Wings, "Content/Items/Accessories/ExampleWings_Wings");
        // Tempo em ticks, velocidade, aceleracao e os cinco parametros verticais.
        Behavior.SetWingStats(180, 9, 2.5f, 0.85f, 0.15f, 1, 3, 0.135f);
    }

    public override void AddRecipes()
    {
        // Nomes locais sao resolvidos por mod; nenhum ID dinamico e fixado aqui.
        CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
    }
}
