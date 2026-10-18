using BunnyLoader.Source;
namespace IndependentMod;
[BunnyContent(BunnyContentKind.Item,"Contract","contract.png")]
public sealed class ContractRecipeItem:BunnyItem {
 public override void SetDefaults(){MaxStack=999;}
 public override void AddRecipes(){
  if(DisplayName!="Contract"||Texture!="contract.png")throw new System.InvalidOperationException("Declared identity not hydrated");
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
