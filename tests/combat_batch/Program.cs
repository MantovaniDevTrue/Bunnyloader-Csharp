using System.Reflection;
using System.Text.Json;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using ExampleModCSharp.CombatHelpers;

var results = new List<object>();
int failures = 0;
void Check(string id, Action action)
{
    try { action(); results.Add(new { id, status = "pass", error = "" }); }
    catch (Exception e) { failures++; results.Add(new { id, status = "fail", error = e.ToString() }); }
}
void Equal<T>(T expected, T actual)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
        throw new Exception($"expected={expected}; actual={actual}");
}
void Contains(string expected, string actual)
{
    if (!actual.Contains(expected, StringComparison.Ordinal))
        throw new Exception($"missing={expected}; actual={actual}");
}
void Reject(Action action)
{
    try { action(); } catch (ArgumentOutOfRangeException) { return; }
    throw new Exception("Expected ArgumentOutOfRangeException");
}
BunnyHurtModifiers Hit() => new() { Damage=40, CurrentLife=30, MaxLife=100,
    NpcIndex=0, AttackerX=-1, AttackerY=-1, AttackerVelocityX=-8,
    AttackerVelocityY=0, AttackerAngle=0, AttackerDistance=160 };

Check("recipe.01.reduction", () => { Equal(30, ReduceHit.Apply(40)); Equal(0, ReduceHit.Apply(1)); });
Check("recipe.02.low-life", () => { Equal(20, LowLifeGuard.Apply(Hit())); var h=Hit(); h.CurrentLife=31; Equal(40,LowLifeGuard.Apply(h)); });
Check("recipe.03.npc-zero", () => Equal(32,NpcContactGuard.Apply(Hit())));
Check("recipe.04.projectile-cap", () => { var h=Hit(); h.NpcIndex=-1; h.ProjectileIndex=0; Equal(20,ProjectileGuard.Apply(h)); h.Damage=8; Equal(8,ProjectileGuard.Apply(h)); });
Check("recipe.05.distance", () => { var h=Hit(); Equal(34,NearbyGuard.Apply(h)); h.AttackerDistance=-1; Equal(40,NearbyGuard.Apply(h)); });
Check("recipe.06.velocity", () => Equal(36,FastAttackGuard.Apply(Hit())));
Check("recipe.07.arc", () => { var h=Hit(); Equal(32,DirectionalGuard.Apply(h)); h.AttackerAngle=MathF.PI; Equal(40,DirectionalGuard.Apply(h)); });
Check("composite.shield", () => {
    var acc = new ExampleModCSharp.ExampleGuardianShieldAccessory();
    acc.SetDefaults();
    Equal("Example Guardian Shield", acc.DisplayName);
    var p = new ExampleModCSharp.ExampleGuardianShieldPlayer();
    p.SetDefaults();
    Equal("ExampleGuardianShieldAccessory", p.Accessory);
    p.ResetEffects();
    p.UpdateEquips(true);
    Equal(5, p.DefenseBonus);
    Equal(4, p.UpdateLifeRegen(true));
    var npc = new BunnyHurtModifiers { Damage=40, NpcIndex=0 };
    p.ModifyHitByNPC(true,0,npc);
    Equal(34,npc.Damage);
    var proj = new BunnyHurtModifiers { Damage=40, ProjectileIndex=0 };
    p.ModifyHitByProjectile(true,0,proj);
    Equal(36,proj.Damage);
    Equal(1,p.NpcBlocks);
    Equal(1,p.ProjectileBlocks);
    var notEquipped = new BunnyHurtModifiers { Damage=40, NpcIndex=0 };
    p.ModifyHitByNPC(false,0,notEquipped);
    Equal(40,notEquipped.Damage);
    p.OnHurt(true,new BunnyHurtInfo(13,30,1,100,false,false));
    Equal(13,p.LastDamageTaken);
    p.ResetEffects();
    p.UpdateEquips(false);
    Equal(0,p.DefenseBonus);
    Equal(0,p.UpdateLifeRegen(false));
});
Check("composite.boots", () => {
    var acc = new ExampleModCSharp.ExampleTrailblazerBootsAccessory();
    acc.SetDefaults();
    Equal("Example Trailblazer Boots",acc.DisplayName);
    var p = new ExampleModCSharp.ExampleTrailblazerBootsPlayer();
    p.SetDefaults();
    Equal("ExampleTrailblazerBootsAccessory",p.Accessory);
    p.ResetEffects();
    p.UpdateEquips(true);
    Equal(12,p.MoveSpeedPercent);
    Equal(8,p.UpdateMovement(true));
    Equal((int)BunnyAccessoryVisualMode.RunningTrail, (int)acc.VisualMode);
    Equal("Content/Items/Accessories/ExampleTrailblazerBootsAccessory_Shoes.png", acc.VisualTexture);
    Equal(false,p.FrameEffects(true));
    p.ResetEffects();
    p.UpdateEquips(false);
    Equal(0,p.MoveSpeedPercent);
    Equal(0,p.UpdateMovement(false));
    Equal(false,p.FrameEffects(false));
});
Check("composite.mystic-ward", () => {
    var acc = new ExampleModCSharp.ExampleMysticWardAccessory();
    acc.SetDefaults();
    Equal("Example Mystic Ward",acc.DisplayName);
    var p = new ExampleModCSharp.ExampleMysticWardPlayer();
    p.SetDefaults();
    Equal("ExampleMysticWardAccessory",p.Accessory);
    p.ResetEffects();
    p.UpdateEquips(true);
    Equal(3,p.DefenseBonus);
    Equal(8,p.UpdateManaRegen(true));
    var npc = new BunnyHurtModifiers { Damage=40, CurrentLife=35, MaxLife=100, NpcIndex=0 };
    p.ModifyHurt(true,npc);
    Equal(32,npc.Damage);
    Equal(1,p.LowLifeGuards);
    var normal = new BunnyHurtModifiers { Damage=40, CurrentLife=36, MaxLife=100, NpcIndex=0 };
    p.ModifyHurt(true,normal);
    Equal(40,normal.Damage);
    var closeProjectile = new BunnyHurtModifiers {
        Damage=32, ProjectileIndex=0, AttackerDistance=192
    };
    p.ModifyHitByProjectile(true,0,closeProjectile);
    Equal(28,closeProjectile.Damage);
    var farProjectile = new BunnyHurtModifiers {
        Damage=32, ProjectileIndex=0, AttackerDistance=193
    };
    p.ModifyHitByProjectile(true,0,farProjectile);
    Equal(32,farProjectile.Damage);
    Equal(1,p.NearbyProjectileGuards);
    p.OnHurt(true,new BunnyHurtInfo(15,30,1,100,false,false,
        npcIndex:-1,projectileIndex:0));
    Equal(1,p.HitsObserved);
    Equal(BunnyHurtSource.Projectile,p.LastHitSource);
    p.ResetEffects();
    p.UpdateEquips(false);
    Equal(0,p.DefenseBonus);
    Equal(0,p.UpdateManaRegen(false));
    var unequipped = new BunnyHurtModifiers { Damage=40, CurrentLife=20, MaxLife=100 };
    p.ModifyHurt(false,unequipped);
    Equal(40,unequipped.Damage);
});
Check("recipe.08.after-hit", () => Contains("origem=Npc danoAplicado=20 vidaRestante=80",
    PostHitSummary.Describe(new BunnyHurtInfo(20,40,1,80,false,false,npcIndex:0))));
Check("source.bounds", () => {
    Equal(BunnyHurtSource.Npc,BunnyCombat.SourceOf(199,0));
    Equal(BunnyHurtSource.Projectile,BunnyCombat.SourceOf(200,999));
    Equal(BunnyHurtSource.Unknown,BunnyCombat.SourceOf(-1,1000));
});
Check("life.exact-boundary", () => {
    Equal(-1,BunnyCombat.LifePercent(1,0)); Equal(100,BunnyCombat.LifePercent(int.MaxValue,1));
    Equal(false,BunnyCombat.IsLowLife(31,101,30)); Equal(true,BunnyCombat.IsLowLife(30,100,30));
    Equal(false,BunnyCombat.IsLowLife(-1,100,30));
});
Check("damage.overflow-and-floor", () => {
    Equal(0,BunnyCombat.ReduceDamage(0,100)); Equal(1,BunnyCombat.ReduceDamage(1,100));
    Equal(0,BunnyCombat.ReduceDamage(1,100,0));
    Equal(int.MaxValue,BunnyCombat.ReduceDamage(int.MaxValue,0));
    Equal(1,BunnyCombat.ReduceDamage(1,10,10));
});
Check("arguments.reject-invalid", () => {
    Reject(()=>BunnyCombat.ReduceDamage(10,101)); Reject(()=>BunnyCombat.CapDamage(-1,2));
    Reject(()=>BunnyCombat.IsWithinDistance(1,float.NaN)); Reject(()=>BunnyCombat.IsInArc(0,0,-1));
});
Check("spatial.sentinels", () => {
    var h=Hit(); Equal(true,BunnyCombat.HasSpatialContext(h));
    h.AttackerDistance=-1; Equal(false,BunnyCombat.HasSpatialContext(h));
    h.AttackerDistance=1; h.AttackerVelocityX=float.PositiveInfinity; Equal(false,BunnyCombat.HasSpatialContext(h));
});
Check("angles.wrap-and-units", () => {
    Equal(true,BunnyCombat.IsInArc(-3.13f,3.13f,0.1f));
    Equal(false,BunnyCombat.IsInArc(float.NaN,0,1));
    Equal(true,Math.Abs(BunnyCombat.NormalizeAngle(2*MathF.PI))<0.00001f);
    Equal(5f,BunnyCombat.Speed(-3,-4));
});
int slot=-1;
Check("runtime.register-error", () => {
    FaultyPlayer.ThrowHook="SetDefaults";
    Equal(-1,BunnyPlayerHurtDispatcher.Register("broken",Assembly.GetExecutingAssembly().Location));
    Contains("class=FaultyPlayer hook=SetDefaults",BunnyPlayerHurtDispatcher.GetLastError(-1));
    FaultyPlayer.ThrowHook="";
    slot=BunnyPlayerHurtDispatcher.Register("fixture",Assembly.GetExecutingAssembly().Location);
    Equal(true,slot>=0);
});
long Modify(int npc=0, int projectile=-1) => BunnyPlayerHurtDispatcher.Modify(slot,40,1,8,30,100,1,
    npc,projectile,1,40,100,1,40,255,-1,-1,-8,0,0,160);
int After(int npc=0, int projectile=-1) => BunnyPlayerHurtDispatcher.After(slot,20,40,1,0,80,1,
    npc,projectile,1,40,100,1,40,255,-1,-1,-8,0,0,160);
foreach(string hook in new[]{"ResetEffects","UpdateEquips","ModifyHurt","ModifyHitByNPC","ModifyHitByProjectile","FreeDodge","OnHurt","OnHitByNPC","OnHitByProjectile"})
    Check("runtime.callback."+hook, () => {
        FaultyPlayer.ThrowHook=hook;
        bool projectile=hook.EndsWith("Projectile");
        long result=hook.StartsWith("On") ? After(projectile?-1:0,projectile?0:-1) : Modify(projectile?-1:0,projectile?0:-1);
        Equal(-1L,result);
        string error=BunnyPlayerHurtDispatcher.GetLastError(slot);
        Contains("mod=fixture class=FaultyPlayer hook="+hook,error);
        Contains("exception=InvalidOperationException message=intentional",error);
        FaultyPlayer.ThrowHook="";
        Equal(true,Modify()>=0);
    });

string scratch=Path.Combine(Path.GetTempPath(),"bunny-batch-"+Guid.NewGuid().ToString("N"));
try
{
    string runtime=Path.Combine(scratch,"runtime"), root=Path.Combine(scratch,"mod");
    Directory.CreateDirectory(runtime); Directory.CreateDirectory(Path.Combine(root,"content"));
    string runtimeSource=Path.GetDirectoryName(typeof(object).Assembly.Location)!;
    foreach(string name in new[]{"System.Private.CoreLib","System.Runtime","netstandard","System.Collections",
        "System.Collections.Concurrent","System.Linq","System.Console","System.Runtime.Extensions","System.ObjectModel",
        "System.Threading","System.Threading.Tasks","System.Numerics.Vectors","System.ComponentModel.Primitives"})
        File.Copy(Path.Combine(runtimeSource,name+".dll"),Path.Combine(runtime,name+".dll"));
    File.Copy(typeof(BunnyPlayer).Assembly.Location,Path.Combine(runtime,"BunnySourceSdk.dll"));
    string output=Path.Combine(scratch,"mod.dll"), diag=Path.Combine(scratch,"diagnostics.txt");
    Check("compiler.512-files-and-cache", () => {
        for(int i=0;i<512;i++) File.WriteAllText(Path.Combine(root,"content",$"Part{i:D2}.cs"),$"public static class Part{i} {{ public const int Value={i}; }}");
        Equal(1,Compiler.Compile(root,output,runtime,diag));
        Equal(2,Compiler.Compile(root,output,runtime,diag));
    });
    Check("compiler.513-files-supported", () => {
        File.WriteAllText(Path.Combine(root,"content","Extra.cs"),"public class Extra {}");
        Equal(1,Compiler.Compile(root,output,runtime,diag));
        Equal(2,Compiler.Compile(root,output,runtime,diag));
        File.Delete(Path.Combine(root,"content","Extra.cs"));
    });
    Check("compiler.actual-error-path-line-code-hint", () => {
        File.WriteAllText(Path.Combine(root,"content","Part00.cs"),"public class Part0\n{\n    public override void Missing() {}\n}");
        Equal(-8,Compiler.Compile(root,output,runtime,diag));
        string error=File.ReadAllText(diag);
        Contains("content/Part00.cs:3:",error); Contains("[CS0115]",error);
        Contains("Como corrigir:",error); Contains("^",error);
    });
    Check("compiler.full-example-package", () => {
        string example=Path.GetFullPath("samples/ExampleModCSharp/Playable");
        int result=Compiler.Compile(example,Path.Combine(scratch,"example.dll"),runtime,diag);
        if(result!=1) throw new Exception(File.ReadAllText(diag));
    });
}
finally { if(Directory.Exists(scratch)) Directory.Delete(scratch,true); }
Directory.CreateDirectory("build");
string json=JsonSerializer.Serialize(new { schemaVersion=1, failures, results },new JsonSerializerOptions{WriteIndented=true});
File.WriteAllText("build/combat-batch-results.json",json);
Console.WriteLine(json);
return failures==0?0:1;

public sealed class FaultyPlayer : BunnyPlayer
{
    public static string ThrowHook="";
    private static void Hit(string hook) { if(ThrowHook==hook) throw new InvalidOperationException("intentional"); }
    public override void SetDefaults() => Hit("SetDefaults");
    public override void ResetEffects() => Hit("ResetEffects");
    public override void UpdateEquips(bool equipped) => Hit("UpdateEquips");
    public override void ModifyHurt(bool equipped,BunnyHurtModifiers hit) => Hit("ModifyHurt");
    public override void ModifyHitByNPC(bool equipped,int index,BunnyHurtModifiers hit) => Hit("ModifyHitByNPC");
    public override void ModifyHitByProjectile(bool equipped,int index,BunnyHurtModifiers hit) => Hit("ModifyHitByProjectile");
    public override bool FreeDodge(bool equipped,BunnyHurtModifiers hit) { Hit("FreeDodge"); return false; }
    public override void OnHurt(bool equipped,BunnyHurtInfo hit) => Hit("OnHurt");
    public override void OnHitByNPC(bool equipped,int index,BunnyHurtInfo hit) => Hit("OnHitByNPC");
    public override void OnHitByProjectile(bool equipped,int index,BunnyHurtInfo hit) => Hit("OnHitByProjectile");
}

