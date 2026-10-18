using BunnyLoader.Source;
namespace IndependentRecipeMod;

public sealed class ContractBolt : BunnyProjectile {
 public override void SetDefaults(){DisplayName="Bolt";Texture="bolt.png";}
}
public sealed class ContractBuff : BunnyBuff {
 public override void SetDefaults(){DisplayName="Buff";Texture="buff.png";DefenseBonus=4;}
}
public sealed class SharedCraftReward : BunnyRecipeBehavior {
 public override bool IsAvailable(BunnyCraftContext context)=>!context.IsDay;
 public override int ConsumeIngredient(BunnyCraftContext context,int type,int count)=>type==9?0:count;
 public override void OnCraft(BunnyCraftContext context){
  if(context.QuickCraft)throw new System.InvalidOperationException("Owned callback failed");
  context.GiveItem<ContractAccessory>();
 }
}
public abstract class WeaponRecipes : BunnyWeapon {
 public override void AddRecipes()=>CreateRecipe().AddIngredient(9,2).AddTile("Bench").AddBehavior(new SharedCraftReward()).Register();
}
[BunnyContent(BunnyContentKind.Weapon,"Contract Weapon","weapon.png")]
public sealed class ContractWeapon : WeaponRecipes {
 public override void SetDefaults(){Shoot=nameof(ContractBolt);Damage=12;UseTime=20;}
}
public sealed class ContractAccessory : BunnyAccessory {
 public override void SetDefaults(){DisplayName="Accessory";Texture="accessory.png";DefenseBonus=3;}
 public override void AddRecipes()=>CreateRecipe().AddIngredient(9,3).AddTile("other/LateBench").AddBehavior(new SharedCraftReward()).Register();
}
public sealed class ContractPotion : BunnyBuffPotion {
 public override void SetDefaults(){DisplayName="Potion";Texture="potion.png";Buff=nameof(ContractBuff);Duration=300;}
 public override void AddRecipes()=>CreateRecipe(2).AddIngredient(9).AddTile(18).AddBehavior(new SharedCraftReward()).Register();
}
