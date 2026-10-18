using BunnyLoader.Source;
namespace IndependentMod;
public sealed class ContractRecipeItem:BunnyItem {
 public override void SetDefaults(){DisplayName="Contract";Texture="contract.png";MaxStack=999;}
 public override void AddRecipes(){
  var group=new BunnyRecipeGroup("ExampleItems","Any example material").AddItem(9).AddItem<ContractRecipeItem>();
  CreateRecipe().AddRecipeGroup(group,3)
   .AddCondition("Low health at night",c=>!c.IsDay&&c.CurrentLife<c.MaxLife/2)
   .AddConsumeIngredientCallback((context,type,amount)=>type==9?0:amount)
   .AddOnCraftCallback(context=>{
    if(context.QuickCraft)throw new System.InvalidOperationException("Fixture callback failure");
    context.GiveItem<ContractRecipeItem>(2);
   }).Register();
 }
}
