using BunnyLoader.Source;
namespace IndependentBuff;
[BunnyContent(BunnyContentKind.Buff,"Computed defense","buff.png")]
public sealed class ComputedBuff:BunnyBuff {
 public override void SetDefaults(){
  DefenseBonus=0;for(int n=0;n<5;n++)DefenseBonus+=2;
  IsDebuff=DefenseBonus>5;NoSave=true;NoTimeDisplay=true;NurseCannotRemove=true;
  Description="Calculated bonus: {0}";MinionProjectile=nameof(ContractProjectile);
 }
}
public sealed class ContractProjectile:BunnyProjectile {
 public override void SetDefaults(){DisplayName="Contract projectile";Texture="projectile.png";Width=18;Height=28;}
}
