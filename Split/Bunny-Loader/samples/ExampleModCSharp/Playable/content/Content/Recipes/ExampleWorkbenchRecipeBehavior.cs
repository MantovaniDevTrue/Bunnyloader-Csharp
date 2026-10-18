using BunnyLoader.Source;

namespace ExampleModCSharp.Recipes;

// Um comportamento pode ser compartilhado por várias receitas de qualquer mod.
// O loader reconstrói esta classe no MonoVM; os hooks recebem snapshots do jogo.
public sealed class ExampleWorkbenchRecipeBehavior : BunnyRecipeBehavior
{
    private static readonly BunnyRecipeCondition available=BunnyRecipeCondition.All(
        "Night and low health",BunnyRecipeCondition.Night,
        new BunnyRecipeCondition("Below half health",HasLowHealth));
    private static bool HasLowHealth(BunnyCraftContext context)
        => context.CurrentLife<context.MaxLife/2;
    public override string ConditionDescription => available.Description;
    public override bool IsAvailable(BunnyCraftContext context) => available.Evaluate(context);
    public override int ConsumeIngredient(BunnyCraftContext context,int ingredientType,int amount)
    {
        // 9 é o placeholder vanilla do grupo Wood. IDs do ExampleMod são resolvidos
        // pelo núcleo; nunca copie seu número temporário mostrado no inventário.
        return ingredientType==9?0:amount;
    }
    public override void OnCraft(BunnyCraftContext context)
    {
        // Só OnCraft pode solicitar itens. A bridge valida todos os comandos e
        // concede o bônus na thread do Terraria após a fabricação normal.
        context.GiveItem<ExampleItem>();
    }
}
