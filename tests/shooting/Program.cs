using BunnyLoader.Source;
using ExampleModCSharp;
using System.Reflection;
using System.Globalization;

int checks=0;
void Require(bool value,string reason){if(!value)throw new Exception(reason);checks++;}
string assembly=Assembly.GetExecutingAssembly().Location;
string input="2,6230,18,42,1136,100,200,10,0,24,7,1,2,3";
string Call(string owner,int kind=1,string? snapshot=null)=>BunnyShootRuntime.Dispatch("independent",assembly,owner,0,kind,snapshot??(kind==2?"":input));
Require(Call("ExampleShotgun",2)=="READY","Prewarm without firing");
Require(Call("ExampleShotgun",0)=="1","Default CanShoot allows");
string output=Call("ExampleShotgun");var parts=output.Split('|');
Require(parts.Length==4&&parts[0]=="SHOT1"&&parts[1]=="0","Eight pellets replace original");
var pellets=parts[3].Split(';');Require(pellets.Length==8,"Original pellet count");
foreach(var text in pellets){var f=text.Split(',');float vx=float.Parse(f[3],CultureInfo.InvariantCulture),vy=float.Parse(f[4],CultureInfo.InvariantCulture);
 Require(f[0]=="v1136"&&f[1]=="100"&&f[2]=="200"&&f[5]=="24"&&f[6]=="7"&&f[7]=="1"&&f[8]=="2"&&f[9]=="3","Selected ammo, damage, position, knockback and AI preserved");
 float speed=MathF.Sqrt(vx*vx+vy*vy);Require(speed>=7&&speed<=10&&MathF.Abs(MathF.Atan2(vy,vx))<=MathF.PI/12,"Original speed and angle distribution");}
Directory.CreateDirectory("build/shooting");File.WriteAllText("build/shooting/shotgun-output.txt",output);
Require(Call("Deny",0)=="0","CanShoot cancellation");
Require(Call("Modify").StartsWith("SHOT1|1|v1136,110,200,10,0,48,7,1,2,3|"),"Modify precedes Shoot");
Require(Call("NoShots").EndsWith("|"),"Zero additional shots");
Require(Call("Many").Split('|')[3].Split(';').Length==64,"64-spawn contract");
Require(Call("Overflow").Contains("SpawnCapacityExceeded"),"Reject whole overflowing output");
Require(Call("Invalid").Contains("ShotOutsideNativeContract"),"Reject non-finite output");
Require(Call("Throws").Contains("original-test-exception")&&Call("Throws").Contains("Exception="),"Original exception preserved");
Require(Call("Named").Contains("manother-mod/Projectile"),"Cross-mod native identity token");
Require(Call("ExampleShotgun",1,input.Replace(",100,",",NaN,")).StartsWith("ERROR|"),"Invalid snapshot");
Require(Call("ExampleShotgun",1,input+",extra").StartsWith("ERROR|"),"Reject extra fields");
Require(Call("EarlySpawn",0).Contains("only available during OnShoot"),"Preflight cannot spawn");
Require(Call("LargeBatch").Length>16384&&Call("LargeBatch").Length<=32768,"64 long identity commands fit the native response budget");
Require(Call("WeaponWithProjectileProperty").StartsWith("SHOT1|0|"),"Legacy BunnyWeapon.Shoot property and new OnShoot callback coexist");
Require(BunnyShootRuntime.Dispatch("independent",assembly,"ExampleShotgun",1,2,"").StartsWith("ERROR|"),"Warmup rejects invalid index");
Require(Call("ExampleShotgun",2,input).StartsWith("ERROR|"),"Warmup rejects payload");
Require(Call("WarmSpy",2)=="READY"&&Call("WarmSpy").Contains(",25,7,1,2,3"),"Warmup never executes user gameplay callbacks");
Require(Call("EscapedContext").Contains("first-context-failure")&&Call("EscapedContext").Contains("only available during OnShoot"),"Failed callbacks cannot leave escaped contexts open");
Require(Call("CrossThreadContext").Contains("WrongCallbackThread"),"Spawn queue enforces callback thread affinity");
Require(new ExampleShotgun().ShootCallbackMask==4,"Actual example override mask");
Require(Call("Hidden",2).Contains("HiddenOrAmbiguousCallback"),"Hidden methods cannot silently bind");
var shotgun=new ExampleShotgun();shotgun.SetDefaults();Require(shotgun.UseTime==55&&shotgun.UseAnimation==55&&shotgun.Damage==10&&shotgun.Value==100000&&shotgun.UseSound=="Item36","Original defaults");
Require(shotgun.NativeDefaultsDefinition.Contains("useAmmo:d:AmmoID:Bullet")&&shotgun.NativeDefaultsDefinition.Contains("shootSpeed:f:10"),"Native ammo and speed");
var gun=new ExampleGun();gun.SetDefaults();
Require(gun.ShootCallbackMask==0&&gun.Damage==20&&gun.UseTime==10&&gun.UseAnimation==10&&gun.AutoReuse&&gun.NoMelee,"Original Gun defaults without fake callbacks");
Require(gun.NativeDefaultsDefinition.Contains("shoot:d:ProjectileID:PurificationPowder")&&gun.NativeDefaultsDefinition.Contains("useAmmo:d:AmmoID:Bullet"),"Gun uses inventory ammunition");
Require(gun.Holdout?.X==-18&&gun.Holdout?.Y==0&&gun.CustomUseSound?.Path=="Assets/Sounds/Items/Guns/ExampleGun.ogg","Gun muzzle offset and asset preserved");
var rocket=new ExampleRocketLauncher();rocket.SetDefaults();
Require(rocket.ShootCallbackMask==0&&rocket.Damage==55&&rocket.UseTime==30&&rocket.Value==400000&&rocket.Rare==8&&rocket.UseSound=="Item11","Original Rocket Launcher defaults without fake callbacks");
Require(rocket.NativeDefaultsDefinition.Contains("shoot:d:ProjectileID:RocketI")&&rocket.NativeDefaultsDefinition.Contains("useAmmo:d:AmmoID:Rocket")&&rocket.NativeDefaultsDefinition.Contains("shootSpeed:f:5"),"Rocket uses native ammunition and speed");
Require(rocket.Holdout?.X==-2&&rocket.Holdout?.Y==-2,"Rocket holdout preserved");
Console.WriteLine($"SHOOT_MANAGED_LIFECYCLE_ORIGINAL_SHOTGUN_AND_FAILURE_ISOLATION_PASS checks={checks}");

public abstract class Fixture:BunnyItem {public override void SetDefaults(){DisplayName="Independent fixture";Texture="not-loaded.png";}}
public sealed class Deny:Fixture {public override bool CanShoot(BunnyShootContext c)=>false;}
public sealed class Modify:Fixture {
 public override void ModifyShootStats(BunnyShootContext c,ref BunnyShot shot){shot.X+=10;shot.Damage*=2;}
 public override bool OnShoot(BunnyShootContext c,BunnyShot shot)=>shot.Damage==48;
}
public sealed class NoShots:Fixture {public override bool OnShoot(BunnyShootContext c,BunnyShot s)=>false;}
public sealed class Many:Fixture {public override bool OnShoot(BunnyShootContext c,BunnyShot s){for(int i=0;i<64;i++)c.Spawn(s);return false;}}
public sealed class Overflow:Fixture {public override bool OnShoot(BunnyShootContext c,BunnyShot s){for(int i=0;i<65;i++)c.Spawn(s);return false;}}
public sealed class Invalid:Fixture {public override void ModifyShootStats(BunnyShootContext c,ref BunnyShot s){s.Damage=2;s.VelocityX=float.NaN;}}
public sealed class Throws:Fixture {public override bool OnShoot(BunnyShootContext c,BunnyShot s)=>throw new Exception("original-test-exception");}
public sealed class Named:Fixture {public override bool OnShoot(BunnyShootContext c,BunnyShot s){s.Projectile="another-mod/Projectile";c.Spawn(s);return false;}}
public sealed class EarlySpawn:Fixture {public override bool CanShoot(BunnyShootContext c){c.Spawn(default);return true;}}
public sealed class Hidden:Fixture {public new bool OnShoot(BunnyShootContext c,BunnyShot s)=>false;}

public sealed class LargeBatch:Fixture {
 public override bool OnShoot(BunnyShootContext c,BunnyShot s){
  s.Projectile=new string('a',80)+"/"+new string('b',79);
  s.X=12345678;s.Y=-12345678;s.VelocityX=0.00012345679f;s.VelocityY=-0.00012345679f;
  s.Damage=1000000;s.KnockBack=0.00000012345679f;s.Ai0=12345678;s.Ai1=-12345678;s.Ai2=-1.2345679E-30f;
  for(int i=0;i<64;i++)c.Spawn(s);return false;
 }
}

public sealed class WeaponWithProjectileProperty:BunnyWeapon {
 public override void SetDefaults(){DisplayName="Independent";Texture="fixture.png";Shoot="IndependentProjectile";}
 public override bool OnShoot(BunnyShootContext context,BunnyShot shot){context.Spawn(shot);return false;}
}

public sealed class WarmSpy:Fixture {
 private int calls;
 public override bool OnShoot(BunnyShootContext context,BunnyShot shot){shot.Damage+=++calls;context.Spawn(shot);return false;}
}

public sealed class EscapedContext:Fixture {
 private BunnyShootContext? previous;
 public override bool OnShoot(BunnyShootContext context,BunnyShot shot){
  if(previous!=null){previous.Spawn(shot);return false;}
  previous=context;throw new Exception("first-context-failure");
 }
}

public sealed class CrossThreadContext:Fixture {
 public override bool OnShoot(BunnyShootContext context,BunnyShot shot){
  Task.Run(()=>context.Spawn(shot)).GetAwaiter().GetResult();return false;
 }
}
