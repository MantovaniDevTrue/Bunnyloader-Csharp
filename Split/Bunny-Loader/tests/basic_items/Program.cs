using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using ExampleModCSharp;
using System.Reflection;
using System.Text.Json;

var results=new List<object>();int failures=0;
void Check(string id,Action f) {try {f();results.Add(new{id,status="pass",error=""});}catch(Exception e){failures++;results.Add(new{id,status="fail",error=e.ToString()});}}
void Equal<T>(T expected,T actual) {if(!EqualityComparer<T>.Default.Equals(expected,actual))throw new Exception($"expected={expected} actual={actual}");}
void Reject(Action f) {try{f();}catch(ArgumentException){return;}catch(InvalidOperationException){return;}throw new Exception("invalid recipe accepted");}
Check("pickaxe.original-defaults",()=>{var item=new ExamplePickaxe();item.SetDefaults();Equal(220,item.Pick);Equal(20,item.Damage);Equal(6f,item.KnockBack);Equal(10,item.UseTime);Equal(10,item.UseAnimation);Equal(true,item.Melee&&item.AutoReuse);Equal("Item1",item.UseSound);Equal(50000,item.Value);item.AddRecipes();Equal("1,18,mExampleItem:10",item.RecipeDefinitions.Single());});
Check("hamaxe.original-defaults",()=>{var item=new ExampleHamaxe();item.SetDefaults();Equal(30,item.Axe);Equal(100,item.Hammer);Equal(25,item.Damage);Equal(15,item.UseTime);Equal(15,item.UseAnimation);Equal(50000,item.Value);Equal("Item1",item.UseSound);item.AddRecipes();Equal("1,18,mExampleItem:10",item.RecipeDefinitions.Single());});
Check("material.original-recipe",()=>{var item=new ExampleItem();item.SetDefaults();Equal(9999,item.MaxStack);Equal(100,item.Value);item.AddRecipes();Equal("999,18,v2:10",item.RecipeDefinitions.Single());});
Check("recipe.reject-invalid",()=>{var item=new ExampleItem();item.SetDefaults();Reject(()=>item.CreateRecipe(0));Reject(()=>item.CreateRecipe().AddIngredient(0));Reject(()=>item.CreateRecipe().AddIngredient("bad,name"));Reject(()=>item.CreateRecipe().AddTile(754));Reject(()=>item.CreateRecipe().Register());Reject(()=>item.CreateRecipe().AddTile(18).AddTile(16));});
Check("recipe.once-and-limits",()=>{var item=new ExampleItem();item.SetDefaults();var r=item.CreateRecipe().AddIngredient(2);r.Register();Reject(()=>r.Register());Reject(()=>r.AddIngredient(3));var many=item.CreateRecipe();for(int i=0;i<15;i++)many.AddIngredient(2);Reject(()=>many.AddIngredient(2));});
Check("probe.items-only-full-wire",()=>{
 Directory.CreateDirectory("build/basic-items");string diag="build/basic-items/probe.txt";
 Equal(3,CompiledAssemblyProbe.Verify(Assembly.GetExecutingAssembly().Location,diag));
 var lines=File.ReadAllLines("build/basic-items/source-runtime-values.txt");Equal("BUNNY_MANAGED_DEFAULTS_V2",lines[0]);Equal(4,lines.Length);
 foreach(var line in lines.Skip(1)) {
  var fields=line.Split('\t');
  if(fields[1]=="ExampleItem") {
   Equal(23,fields.Length);
   if(!fields[22].StartsWith("pt-BR:"))throw new Exception("Missing native pt-BR item tooltip");
  } else Equal(20,fields.Length);
 }
 if(!lines.Any(l=>l.Contains("\tItem1\t")))throw new Exception("native sound missing");
});
string scratch=Path.Combine(Path.GetTempPath(),"bunny-item-"+Guid.NewGuid().ToString("N"));
try {
 Directory.CreateDirectory(scratch);string runtime=Path.Combine(scratch,"runtime");Directory.CreateDirectory(runtime);
 string source=Path.GetDirectoryName(typeof(object).Assembly.Location)!;
 foreach(string name in new[]{"System.Private.CoreLib","System.Runtime","netstandard","System.Collections","System.Linq","System.Console","System.Runtime.Extensions","System.ObjectModel","System.Collections.Concurrent","System.Threading","System.Threading.Tasks"})
  File.Copy(Path.Combine(source,name+".dll"),Path.Combine(runtime,name+".dll"));
 File.Copy(typeof(BunnyItem).Assembly.Location,Path.Combine(runtime,"BunnySourceSdk.dll"));
 Check("roslyn.auto-items-only-package",()=>{
  string mod=Path.Combine(scratch,"mod");Directory.CreateDirectory(Path.Combine(mod,"content"));
  foreach(string path in new[]{"Content/Items/ExampleItem.cs","Content/Items/Tools/ExamplePickaxe.cs","Content/Items/Tools/ExampleHamaxe.cs"})
   File.Copy(Path.Combine("samples/ExampleModCSharp/Playable/content",path),Path.Combine(mod,"content",Path.GetFileName(path)));
  string output=Path.Combine(scratch,"items.dll"),diag=Path.Combine(scratch,"compile.txt");
  Equal(1,Compiler.Compile(mod,output,runtime,diag));Equal(3,CompiledAssemblyProbe.Verify(output,diag));Equal(2,Compiler.Compile(mod,output,runtime,diag));
 });
 Check("roslyn.full-source-package",()=>{
  string root="samples/ExampleModCSharp/Playable",output=Path.Combine(scratch,"all.dll"),diag=Path.Combine(scratch,"all.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));
  var domains=new Dictionary<Type,string>{
   [typeof(BunnyItem)]="ITEM",[typeof(BunnyWeapon)]="ITEM",[typeof(BunnyAccessory)]="ITEM",[typeof(BunnyBuffPotion)]="ITEM",
   [typeof(BunnyTile)]="TILE",[typeof(BunnyProjectile)]="PROJECTILE",[typeof(BunnyNPC)]="NPC",
   [typeof(BunnyBuff)]="BUFF",[typeof(BunnyPlayer)]="PLAYER",[typeof(BunnyPrefix)]="PREFIX"
  };
  var compiled=Assembly.Load(File.ReadAllBytes(output));
  var identities=compiled.GetTypes().Where(t=>t.IsClass&&!t.IsAbstract&&!t.ContainsGenericParameters)
   .Select(t=>new{type=t,domain=domains.FirstOrDefault(p=>p.Key.IsAssignableFrom(t)).Value})
   .Where(x=>x.domain is not null).Select(x=>x.domain+":"+x.type.Name).ToHashSet(StringComparer.Ordinal);
  if(identities.Count==0)throw new Exception("full package has no content");
  Equal(identities.Count,CompiledAssemblyProbe.Verify(output,diag));
  var actual=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt")).Skip(1)
   .Where(l=>l.Length>0).Select(l=>l.Split('\t')).Select(f=>
    (f[0] is "WEAPON" or "ACCESSORY" or "POTION"?"ITEM":f[0])+":"+f[1]).ToArray();
  Equal(identities.Count,actual.Length);
  if(!identities.SetEquals(actual))throw new Exception("compiled types and native wire identities differ");
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/full-source-runtime-values.txt",true);
  var assets=new List<object>();
  foreach(var type in compiled.GetTypes().Where(t=>t.IsClass&&!t.IsAbstract&&!t.ContainsGenericParameters&&
      domains.Keys.Any(baseType=>baseType.IsAssignableFrom(t)))) {
    var instance=Activator.CreateInstance(type)??throw new Exception("Cannot instantiate "+type.FullName);
    var defaults=type.GetMethod(typeof(BunnyTile).IsAssignableFrom(type)?"SetStaticDefaults":"SetDefaults");
    defaults?.Invoke(instance,null);
    void RequireAsset(string? name) {
      if(!string.IsNullOrWhiteSpace(name))
        assets.Add(new {path=name,className=type.Name,file="ExecutedCSharpDefaults"});
    }
    foreach(var propertyName in new[]{"Texture","VisualTexture"})
      RequireAsset(type.GetProperty(propertyName)?.GetValue(instance) as string);
    if(type.GetProperty("BehaviorDefinition")?.GetValue(instance) is string policy&&policy.Length>0) {
      using var descriptor=JsonDocument.Parse(policy);
      if(descriptor.RootElement.TryGetProperty("equips",out var equips))
        foreach(var equip in equips.EnumerateArray())
          if(equip.GetArrayLength()>1)RequireAsset(equip[1].GetString());
    }
  }
  if(assets.Count==0)throw new Exception("Executed C# defaults yielded no asset references");
  File.WriteAllText("build/basic-items/full-source-assets.json",JsonSerializer.Serialize(assets));
  if(new FileInfo(Path.Combine(scratch,"source-runtime-values.txt")).Length>8*1024*1024)throw new Exception("wire exceeds runtime budget");
 });
 Check("behavior.public-sdk-validation",()=>{
  var owner=new ExampleItem();owner.SetDefaults();
  owner.Behavior.MultiplyOnEquip(BunnyEquipFloat.MeleeDamage,1.1f);
  owner.Behavior.SetOnEquip(BunnyEquipFlag.StrongBees);
  Equal("",owner.BehaviorDefinition);
  Equal("meleeDamage:mul:1.1;strongBees:set:1",owner.NativeAccessoryEffectsDefinition);
  Reject(()=>owner.Behavior.EquipTexture(BunnyEquipKind.Head,"../escape"));
  Reject(()=>owner.Behavior.Animate(0,6));
  Reject(()=>owner.Behavior.WorldLight(float.NaN,0,0));
  Reject(()=>owner.Behavior.MultiplyOnEquip(BunnyEquipFloat.MeleeDamage,-1));
 });
 Check("placement.public-sdk-native-defaults",()=>{
  var owner=new ExampleItem();owner.SetDefaults();owner.DefaultToPlaceableTile("other/Bench",3);
  Equal("mother/Bench:3",owner.PlacementDefinition);Equal(1,owner.UseStyle);Equal(15,owner.UseAnimation);
  Equal(10,owner.UseTime);Equal(9999,owner.MaxStack);Equal(true,owner.UseTurn);Equal(true,owner.AutoReuse);Equal(true,owner.Consumable);
  Reject(()=>owner.DefaultToPlaceableTile("mod//Tile"));Reject(()=>owner.DefaultToPlaceableTile("../Tile"));
  Reject(()=>owner.DefaultToPlaceableTile(754));Reject(()=>owner.DefaultToPlaceableTile(18,-1));
  owner.DefaultToPlaceableTile(18,0);Equal("v18:0",owner.PlacementDefinition);
});
 Check("recipe.mod-station-public-sdk",()=>{
  var owner=new ExampleItem();owner.SetDefaults();
  owner.CreateRecipe().AddIngredient("ExampleItem").AddTile("ExampleWorkbench").Register();
  Equal("1,mExampleWorkbench,mExampleItem:1",owner.RecipeDefinitions.Single());
  Reject(()=>owner.CreateRecipe().AddTile("../bad"));Reject(()=>owner.CreateRecipe().AddTile("/bad"));
  Reject(()=>owner.CreateRecipe().AddTile("mod//bad"));Reject(()=>owner.CreateRecipe().AddTile("mod/a/b"));
  Reject(()=>owner.CreateRecipe().AddTile("Station").AddTile(18));
 });
 Check("armor.public-sdk-contract",()=>{
  var item=new ExampleItem();item.SetDefaults();item.Defense=6;item.Behavior.EquipTexture(BunnyEquipKind.Head,"Contract/Head");
  item.Behavior.Armor.Add(BunnyArmorInteger.StatManaMax2,20);
  item.Behavior.Armor.ImmuneTo("OnFire");
  var set=new BunnyArmorSet("Head","Plate","Legs","quote: \"armor\"");set.Effects.Add(BunnyArmorStat.MeleeDamage,0.2f);
  item.Behavior.DefineArmorSet(set);
  using var json=JsonDocument.Parse(item.BehaviorDefinition);
  Equal(6,json.RootElement.GetProperty("defense").GetInt32());
  Equal("quote: \"armor\"",json.RootElement.GetProperty("armorSet").GetProperty("text").GetString());
  Reject(()=>item.Defense=-1);Reject(()=>item.Defense=1001);
  Reject(()=>item.Behavior.Armor.Add((BunnyArmorStat)999,0.1f));
  Reject(()=>item.Behavior.Armor.Add(BunnyArmorStat.MoveSpeed,float.NaN));
  Reject(()=>item.Behavior.Armor.ImmuneTo("not a buff"));
  Reject(()=>item.Behavior.DefineArmorSet(set));
  Reject(()=>new BunnyArmorSet("Head","Body","Legs","invalid\ntext"));
 });
 Check("wings.real-source-wire",()=>{
  string root="tests/basic_items/wing_fixture",output=Path.Combine(scratch,"wing.dll"),diag=Path.Combine(scratch,"wing.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(1,CompiledAssemblyProbe.Verify(output,diag));
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/wing-source-runtime-values.txt",true);
  var item=new ExampleItem();item.SetDefaults();Reject(()=>item.Behavior.SetWingStats(0,9,2.5f,.85f,.15f,1,3,.135f));
  Reject(()=>item.Behavior.SetWingStats(180,float.NaN,2.5f,.85f,.15f,1,3,.135f));
 });
 Check("placement.android-probe-source-wire",()=>{
  string root="samples/NativePlacementProbe/Playable",output=Path.Combine(scratch,"probe.dll"),diag=Path.Combine(scratch,"probe.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(3,CompiledAssemblyProbe.Verify(output,diag));
  var lines=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));Equal(4,lines.Length);
  var actual=lines.Skip(1).Select(l=>l.Split('\t')).ToDictionary(f=>f[1]);
  Equal("v18:0",actual["ProbeWorkbench"][21]);Equal("v19:0",actual["ProbePlatformA"][21]);Equal("v19:1",actual["ProbePlatformB"][21]);
  foreach(var f in actual.Values){Equal(22,f.Length);Equal("10,-1,v2:1",f[19]);Equal("-",f[20]);}
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/probe-source-runtime-values.txt",true);
 });
 Check("placement.real-source-wire",()=>{
  string root="tests/basic_items/placeable_fixture",output=Path.Combine(scratch,"placement.dll"),diag=Path.Combine(scratch,"placement.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(1,CompiledAssemblyProbe.Verify(output,diag));
  var lines=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));Equal(2,lines.Length);
  var fields=lines[1].Split('\t');Equal(22,fields.Length);Equal("-",fields[20]);Equal("mother/Workbench:3",fields[21]);
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/placement-source-runtime-values.txt",true);
 });
 Check("armor.real-source-wire",()=>{
  string root="tests/basic_items/armor_fixture",output=Path.Combine(scratch,"armor.dll"),diag=Path.Combine(scratch,"armor.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(3,CompiledAssemblyProbe.Verify(output,diag));
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/armor-source-runtime-values.txt",true);
  var lines=File.ReadAllLines("build/basic-items/armor-source-runtime-values.txt");Equal(4,lines.Length);
  foreach(var line in lines.Skip(1))Equal(26,line.Split('\t').Length);
  var helmet=lines.Single(line=>line.StartsWith("ITEM\tContractHelmet\t")).Split('\t');Equal("1,mContractBench,v2:1",helmet[19]);
 });
 Check("identity.same-name-distinct-native-domains",()=>{
  string root=Path.Combine(scratch,"dual-identity");Directory.CreateDirectory(Path.Combine(root,"content"));
  File.WriteAllText(Path.Combine(root,"content","Item.cs"),
    "using BunnyLoader.Source; namespace Example.Items; public sealed class ExampleWorkbench:BunnyItem { public override void SetDefaults(){DisplayName=\"Example Workbench Item\";Texture=\"Content/Items/ExampleWorkbench.png\";MaxStack=9999;} }");
  File.WriteAllText(Path.Combine(root,"content","Projectile.cs"),
    "using BunnyLoader.Source; namespace Example.Projectiles; public sealed class ExampleWorkbench:BunnyProjectile { public override void SetDefaults(){DisplayName=\"Example Workbench Projectile\";Texture=\"Content/Projectiles/ExampleWorkbench.png\";Width=12;Height=12;AiStyle=1;Friendly=true;Penetrate=1;TimeLeft=240;} }");
  string output=Path.Combine(scratch,"dual-identity.dll"),diag=Path.Combine(scratch,"dual-identity.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(2,CompiledAssemblyProbe.Verify(output,diag));
  var records=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));
  Equal(3,records.Length);
  if(!records.Any(line=>line.StartsWith("ITEM\tExampleWorkbench\t")) ||
     !records.Any(line=>line.StartsWith("PROJECTILE\tExampleWorkbench\t")))
      throw new Exception("native domain identity dropped a class");
 });
 Check("tiles.original-bench-and-item-same-name-wire",()=>{
  string root="tests/basic_items/tile_fixture";
  string output=Path.Combine(scratch,"tile.dll"),diag=Path.Combine(scratch,"tile.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));
  Equal(6,CompiledAssemblyProbe.Verify(output,diag));
  var rows=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));
  Equal(7,rows.Length);
  var exampleMaterial=rows.Single(row=>row.StartsWith("ITEM\tExampleItem\t")).Split('\t');
  Equal("ExampleItem",exampleMaterial[1]);
  var tile=rows.Single(row=>row.StartsWith("TILE\tExampleWorkbench\t")).Split('\t');
  var item=rows.Single(row=>row.StartsWith("ITEM\tExampleWorkbench\t")).Split('\t');
  Equal(13,tile.Length); Equal("1",tile[3]);Equal("18",tile[4]);Equal("18",tile[5]);
  Equal("Platinum",tile[6]);Equal("200",tile[7]);Equal("200",tile[8]);Equal("200",tile[9]);
  Equal("1",tile[11]);Equal("3",tile[12]);
  Equal(22,item.Length);Equal("mExampleWorkbench:0",item[21]);
  Equal("1,-1,v36:1,mExampleItem:10",item[19]);
  foreach (string armorName in new[]{"ExampleHelmet","ExampleBreastplate","ExampleLeggings"}) {
   var values=rows.Single(row=>row.StartsWith("ITEM\t"+armorName+"\t")).Split('\t');
   Equal(26,values.Length);Equal("-",values[22]);Equal((armorName=="ExampleBreastplate"?6:5).ToString(),values[23]);
   Equal("1,mExampleWorkbench,mExampleItem:1",values[19]);
   using var behavior=JsonDocument.Parse(Convert.FromHexString(values[20]));
   var rootBehavior=behavior.RootElement;
   Equal(armorName=="ExampleBreastplate"?6:5,rootBehavior.GetProperty("defense").GetInt32());
   if(armorName=="ExampleHelmet") {
    var set=rootBehavior.GetProperty("armorSet");
    Equal("ExampleHelmet",set.GetProperty("head").GetString());
    Equal("ExampleBreastplate",set.GetProperty("body").GetString());
    Equal("ExampleLeggings",set.GetProperty("legs").GetString());
    Equal(4,set.GetProperty("effects").GetProperty("effects").GetArrayLength());
    Equal(0.2f,set.GetProperty("effects").GetProperty("effects")[0][2].GetSingle());
   } else if(armorName=="ExampleBreastplate") {
    var effects=rootBehavior.GetProperty("armor");
    Equal(2,effects.GetProperty("effects").GetArrayLength());
    Equal("OnFire",effects.GetProperty("immune")[0].GetString());
   } else {
    var effects=rootBehavior.GetProperty("armor").GetProperty("effects");
    Equal("moveSpeed",effects[0][0].GetString());
    Equal(0.05f,effects[0][2].GetSingle());
   }
  }
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/tiles-source-runtime-values.txt",true);
 });
 Check("defense.only-native-wire-without-js-policy",()=>{
  string root=Path.Combine(scratch,"defense-native");Directory.CreateDirectory(Path.Combine(root,"content"));
  File.WriteAllText(Path.Combine(root,"content","BareArmor.cs"),
    "using BunnyLoader.Source; public sealed class BareArmor:BunnyItem { public override void SetDefaults(){DisplayName=\"Bare Armor\";Texture=\"Content/Items/BareArmor.png\";Defense=7;} }");
  string output=Path.Combine(scratch,"defense-native.dll"),diag=Path.Combine(scratch,"defense-native.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(1,CompiledAssemblyProbe.Verify(output,diag));
  var lines=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));
  Equal(2,lines.Length);
  var fields=lines[1].Split('\t');
  Equal("ITEM",fields[0]);Equal("BareArmor",fields[1]);Equal(24,fields.Length);
  Equal("-",fields[20]);Equal("-",fields[21]);Equal("-",fields[22]);Equal("7",fields[23]);
 });
 Check("compiler.192-content-classes-and-expanded-wire",()=>{
  string root=Path.Combine(scratch,"capacity");Directory.CreateDirectory(Path.Combine(root,"content"));
  for(int i=0;i<192;i++)File.WriteAllText(Path.Combine(root,"content",$"Item{i:D3}.cs"),
    $"using BunnyLoader.Source; public sealed class Item{i:D3}:BunnyItem {{ public override void SetDefaults() {{ DisplayName=\"Item\"; Texture=\"Items/Item\"; MaxStack=9999; }} }}");
  string output=Path.Combine(scratch,"capacity.dll"),diag=Path.Combine(scratch,"capacity.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(192,CompiledAssemblyProbe.Verify(output,diag));
  var wire=File.ReadAllText(Path.Combine(scratch,"source-runtime-values.txt"));
  if(wire.Length<=8192||wire.Length>2*1024*1024)throw new Exception("192 descriptors did not exercise expanded bounded wire");
  File.WriteAllText("build/basic-items/capacity-source-runtime-values.txt",wire);
  for(int i=192;i<513;i++)File.WriteAllText(Path.Combine(root,"content",$"Extra{i:D3}.cs"),"// helper");
  Equal(1,Compiler.Compile(root,output,runtime,diag));
  Equal(192,CompiledAssemblyProbe.Verify(output,diag));
  Equal(2,Compiler.Compile(root,output,runtime,diag));
 });
} finally {if(Directory.Exists(scratch))Directory.Delete(scratch,true);}
Directory.CreateDirectory("build");string json=JsonSerializer.Serialize(new{schemaVersion=1,failures,results},new JsonSerializerOptions{WriteIndented=true});File.WriteAllText("build/basic-items-results.json",json);Console.WriteLine(json);return failures==0?0:1;
