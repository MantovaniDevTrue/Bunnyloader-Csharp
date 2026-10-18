using System;
using BunnyLoader.Source;
namespace RecipeBehaviorContract;

public abstract class SharedRules : BunnyRecipeBehavior
{
    private static readonly BunnyRecipeCondition available=CreateCondition();
    private static BunnyRecipeCondition CreateCondition()
    {
        var parts=new[]{BunnyRecipeCondition.Night,BunnyRecipeCondition.NearWater};
        var condition=BunnyRecipeCondition.All("Night near water",parts);
        parts[0]=BunnyRecipeCondition.Day; // Composition snapshots the caller's array.
        return condition;
    }
    public override bool IsAvailable(BunnyCraftContext c)
        => available.Evaluate(c);
    public override int ConsumeIngredient(BunnyCraftContext c,int type,int amount)
        => type==9?0:amount;
}
public sealed class Rules : SharedRules
{
    public override void OnCraft(BunnyCraftContext c)
    {
        if(c.Stage!=BunnyRecipeStage.Craft)throw new Exception("Wrong stage");
        c.GiveItem("BehaviorItem");
    }
}
public sealed class BehaviorItem : BunnyItem
{
    public override void SetDefaults(){DisplayName="Behavior Item";Texture="BehaviorItem.png";}
    public override void AddRecipes()
    {
        var recipe=CreateRecipe().AddIngredient(9,2).AddBehavior(new Rules());
        recipe.Register();recipe.Clone().AddOnCraftCallback(c=>c.GiveItem("BehaviorItem",2)).Register();
        CreateRecipe().AddIngredient(9).AddCondition("Forbidden grant",c=>{c.GiveItem(9);return true;}).Register();
        CreateRecipe().AddIngredient(9).AddConsumeIngredientCallback((c,t,n)=>{c.GiveItem(9);return n;}).Register();
        CreateRecipe().AddIngredient(9).AddCondition(BunnyRecipeCondition.Any("Day or water",
            BunnyRecipeCondition.Day,BunnyRecipeCondition.NearWater).Not("Night and dry")).Register();
    }
}
public sealed class UnrelatedBrokenItem : BunnyItem
{
    public override void SetDefaults()=>throw new InvalidOperationException("UnrelatedOwnerFailure");
}
