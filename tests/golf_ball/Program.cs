using BunnyLoader.Source;
using ExampleModCSharp;
using System.Reflection;

static void Check(bool valid, string why) { if(!valid)throw new Exception(why); }
static void Reject(Action action) {
    try { action(); } catch(ArgumentException) { return; }
    throw new Exception("A política inválida foi aceita");
}
var ball=new ExampleGolfBall(); ball.SetDefaults();
// Valores conferidos no helper nativo Item.DefaultToGolfBall, Android 301720.
Check(ball.Width==18 && ball.Height==20 && ball.MaxStack==1,"Dimensões/pilha do helper nativo");
Check(ball.UseStyle==1 && ball.UseTime==15 && ball.UseAnimation==15,"Uso da bola");
Check(ball.Accessory && ball.NoMelee && !ball.Consumable,"Bola equipável não consumível");
Check(ball.Rare==2 && ball.Value==10000 && ball.UseSound=="Item1","Raridade, preço e som do helper");
Check(ball.NativeDefaultsDefinition=="hasVanityEffects:b:1;noUseGraphic:b:1;shoot:p:ExampleGolfBallProjectile;shootSpeed:f:12","Defaults nativos da bola");
var projectile=new ExampleGolfBallProjectile(); projectile.SetDefaults(); projectile.SetStaticDefaults();
Check(projectile.Width==7 && projectile.Height==7 && projectile.AiStyle==149,"Física GolfBall nativa");
Check(projectile.Friendly && projectile.Penetrate==-1 && !projectile.TileCollide && !projectile.IgnoreWater && projectile.TimeLeft==3600,"Flags e duração do projétil original");
Check(projectile.NativeDefaultsDefinition=="ProjectileSet_IsAGolfBall:b:1;ProjectileSet_TrailCacheLength:i:20;ProjectileSet_TrailingMode:i:0;netImportant:b:1;ranged:b:0","Tabelas de golfe e histórico");
foreach(Type type in new[]{typeof(ExampleGolfBall),typeof(ExampleGolfBallProjectile)}) {
    var names=type.GetCustomAttributes<BunnyNameAttribute>().ToDictionary(n=>n.Culture,n=>n.Text);
    Check(names["en-US"]=="Example Golf Ball" && names["pt-BR"]=="Bola de Golfe de Exemplo","Nomes originais localizados");
}
Check(projectile.Texture=="Content/Projectiles/ExampleGolfBallProjectile.png" && ball.Texture=="Content/Items/ExampleGolfBall.png","Assets originais");
Reject(()=>new BunnyNativeDefaults().SetProjectileStaticBool("NoSuchTable",true));
Reject(()=>new BunnyNativeDefaults().SetProjectileStaticInt("IsAGolfBall",1));
foreach(int bad in new[]{-1,129})Reject(()=>new BunnyNativeDefaults().SetProjectileStaticInt("TrailCacheLength",bad));
foreach(int bad in new[]{-1,4})Reject(()=>new BunnyNativeDefaults().SetProjectileStaticInt("TrailingMode",bad));
Reject(()=>ball.DefaultToGolfBall("../other"));
Console.WriteLine("GOLF_BALL_MANAGED_DEFAULTS=PASS");
