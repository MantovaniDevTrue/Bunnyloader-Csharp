using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using ExampleModCSharp;
using System.Reflection;
using System.Text.Json;

var results=new List<object>();int failures=0;
void Check(string id,Action f) {try {f();results.Add(new{id,status="pass",error=""});}catch(Exception e){failures++;results.Add(new{id,status="fail",error=e.ToString()});}}
void Equal<T>(T expected,T actual) {if(!EqualityComparer<T>.Default.Equals(expected,actual))throw new Exception($"expected={expected} actual={actual}");}
void Reject(Action f) {try{f();}catch(ArgumentException){return;}catch(InvalidOperationException){return;}throw new Exception("invalid recipe accepted");}
Check("native-content.buy-sell-price-terraria-units",()=>{
 Equal(400000,BunnyItem.BuyPrice(gold:40));Equal(50000,BunnyItem.SellPrice(gold:1));Equal(5000,BunnyItem.SellPrice(silver:10));
 Reject(()=>BunnyItem.BuyPrice(copper:-1));
 bool overflow=false;try{BunnyItem.SellPrice(platinum:int.MaxValue);}catch(OverflowException){overflow=true;}Equal(true,overflow);
});
Check("native-defaults.public-builder-values-and-invalid-input",()=>{
 var defaults=new BunnyNativeDefaults().SetInt("mana",10).SetBool("channel",true).SetFloat("shootSpeed",4.5f)
   .SetProjectile("shoot","another-mod/Projectile").SetId("ammo","AmmoID","Bullet");
 Equal(true,defaults.Definition.Contains("mana:i:10")&&defaults.Definition.Contains("channel:b:1"));
 Reject(()=>defaults.SetFloat("light",float.NaN));Reject(()=>defaults.SetInt("bad;field",1));
 Reject(()=>defaults.SetId("ammo","ArbitraryClass","Pointer"));Reject(()=>defaults.SetProjectile("shoot","bad/key/again"));
 Equal("mana:i:20",new BunnyNativeDefaults().SetInt("mana",10).SetInt("mana",20).Definition);
});
Check("sound.immutable-options-paths-and-original-gun",()=>{
 var sound=new BunnySoundStyle("Assets/Sounds/Items/Guns/ExampleGun.ogg",volume:0.9f,pitchVariance:0.2f,maxInstances:3);
 Equal(0.9f,sound.Volume);Equal(0.2f,sound.PitchVariance);Equal(3,sound.MaxInstances);
 Equal("Assets/Sounds/Items/Guns/ExampleGun.ogg",sound.Path);
 Equal(6,sound.Definition.Split('\t').Length);
 foreach(var invalid in new[]{"../bad.ogg","/bad.wav","foo/../bad.ogg","bad.mp4","bad\\path.ogg","bad\tfile.wav"})Reject(()=>new BunnySoundStyle(invalid));
 Reject(()=>new BunnySoundStyle("ok.wav",volume:float.NaN));Reject(()=>new BunnySoundStyle("ok.wav",pitch:float.PositiveInfinity));
 Reject(()=>new BunnySoundStyle("ok.wav",maxInstances:-1));Reject(()=>new BunnySoundStyle("ok.wav",limitBehavior:(BunnySoundLimitBehavior)3));
});
Check("pickaxe.original-defaults",()=>{var item=new ExamplePickaxe();item.SetDefaults();Equal(220,item.Pick);Equal(20,item.Damage);Equal(6f,item.KnockBack);Equal(10,item.UseTime);Equal(10,item.UseAnimation);Equal(true,item.Melee&&item.AutoReuse);Equal("Item1",item.UseSound);Equal(50000,item.Value);item.AddRecipes();Equal("1,18,mExampleItem:10",item.RecipeDefinitions.Single());});
Check("hamaxe.original-defaults",()=>{var item=new ExampleHamaxe();item.SetDefaults();Equal(30,item.Axe);Equal(100,item.Hammer);Equal(25,item.Damage);Equal(15,item.UseTime);Equal(15,item.UseAnimation);Equal(50000,item.Value);Equal("Item1",item.UseSound);item.AddRecipes();Equal("1,18,mExampleItem:10",item.RecipeDefinitions.Single());});
Check("material.original-recipe",()=>{var item=new ExampleItem();item.SetDefaults();Equal(9999,item.MaxStack);Equal(100,item.Value);item.AddRecipes();Equal("999,18,v2:10",item.RecipeDefinitions.Single());});
Check("minion.configurable-movement-and-finite-state",()=>{
 var custom=new BunnyMinionMovement(attackSpeed:16f,attackInertia:1f,followOffsetX:0f,followOffsetY:0f);
 var state=new BunnyProjectileAiState{TargetNpc=7,TargetX=100f,TargetY=0f};
 Equal(true,BunnyMinionSteering.FollowOrAttack(ref state,custom));Equal(16f,state.VelocityX);Equal(0f,state.VelocityY);
 state=new BunnyProjectileAiState{TargetNpc=-1,VelocityX=10f,VelocityY=-10f};
 Equal(true,BunnyMinionSteering.FollowOrAttack(ref state,custom));Equal(8.2f,state.VelocityX);Equal(-8.2f,state.VelocityY);
 state.TargetNpc=7;state.TargetX=float.NaN;var previous=state;Equal(false,BunnyMinionSteering.FollowOrAttack(ref state,custom));Equal(previous.VelocityX,state.VelocityX);
 Reject(()=>new BunnyMinionMovement(attackSpeed:float.NaN));Reject(()=>new BunnyMinionMovement(followInertia:0));
 Reject(()=>new BunnyMinionMovement(idleDistance:700,catchUpDistance:600));Reject(()=>new BunnyMinionMovement(idleDamping:2));
});
Check("recipe.reject-invalid",()=>{var item=new ExampleItem();item.SetDefaults();Reject(()=>item.CreateRecipe(0));Reject(()=>item.CreateRecipe().AddIngredient(0));Reject(()=>item.CreateRecipe().AddIngredient("bad,name"));Reject(()=>item.CreateRecipe().AddTile(754));Reject(()=>item.CreateRecipe().Register());Reject(()=>item.CreateRecipe().AddTile(18).AddTile(16));});
Check("recipe.once-and-limits",()=>{var item=new ExampleItem();item.SetDefaults();var r=item.CreateRecipe().AddIngredient(2);r.Register();Reject(()=>r.Register());Reject(()=>r.AddIngredient(3));var many=item.CreateRecipe();for(int i=0;i<15;i++)many.AddIngredient(2);Reject(()=>many.AddIngredient(2));});
Check("recipe.native-group-public-sdk",()=>{
 var item=new ExampleItem();item.SetDefaults();
 item.CreateRecipe().AddNativeRecipeGroup("Wood",10).AddNativeRecipeGroup("IronBar",2).AddIngredient<ExampleItem>().AddTile("ExampleWorkbench").Register();
 Equal("1,mExampleWorkbench,gWood:10,gIronBar:2,mExampleItem:1",item.RecipeDefinitions.Single());
 foreach(string bad in new[]{"","../Wood","Wood:1","Wood/Other","1Wood","Wood;IronBar",new string('x',73)})Reject(()=>item.CreateRecipe().AddNativeRecipeGroup(bad));
 Reject(()=>item.CreateRecipe().AddNativeRecipeGroup("Wood",0));
 Reject(()=>item.CreateRecipe().AddNativeRecipeGroup("Wood").AddNativeRecipeGroup("Wood"));
 Reject(()=>item.CreateRecipe().AddIngredient<object>());Reject(()=>item.CreateRecipe().AddIngredient<BunnyItem>());
 Reject(()=>item.CreateRecipe().AddTile<ExampleItem>());
 var once=item.CreateRecipe().AddNativeRecipeGroup("Wood");once.Register();Reject(()=>once.AddNativeRecipeGroup("IronBar"));
 var many=item.CreateRecipe();for(int i=0;i<15;i++)many.AddIngredient(2);Reject(()=>many.AddNativeRecipeGroup("Wood"));
});
Check("recipe.custom-group-builder-snapshot",()=>{
 var item=new ExampleItem();item.SetDefaults();
 var group=new BunnyRecipeGroup("AnyMaterial","Any material").AddItem(9).AddItem<ExampleItem>();
 var recipe=item.CreateRecipe().AddRecipeGroup(group,3);group.AddItem(22);recipe.Register();
 Equal("1,-1,hAnyMaterial~416E79206D6174657269616C~v9+mExampleItem:3",item.RecipeDefinitions[0]);
 Reject(()=>new BunnyRecipeGroup("bad/name","Description"));Reject(()=>new BunnyRecipeGroup("Group","").AddItem(2));
 Reject(()=>new BunnyRecipeGroup("Group","Text").AddItem(2).AddItem(2));
 Reject(()=>item.CreateRecipe().AddRecipeGroup(new BunnyRecipeGroup("Empty","Text")));
 Reject(()=>recipe.AddCondition("test",c=>true));Reject(()=>recipe.AddOnCraftCallback(c=>{}));
});
Check("recipe.behavior-validation-atomicity",()=>{
 var item=new ExampleItem();item.SetDefaults();
 Reject(()=>item.CreateRecipe().AddBehavior(new EmptyBehavior()));
 Reject(()=>item.CreateRecipe().AddBehavior(new HiddenBehavior()));
 Reject(()=>new BunnyRecipeCondition("bad\nlabel",c=>true));
 Reject(()=>BunnyRecipeCondition.All("Empty"));
 var recipe=item.CreateRecipe().AddIngredient(9);
 for(int i=0;i<16;i++)recipe.AddOnCraftCallback(c=>{});
 Reject(()=>recipe.AddBehavior(new FullBehavior()));recipe.Register();
 if(!item.RecipeDefinitions[0].EndsWith("@4"))throw new Exception("Failed binding partially changed callbacks");
});
Check("recipe.condition-clone-removal",()=>{
 var item=new ExampleItem();item.SetDefaults();var night=BunnyRecipeCondition.Night;
 var original=item.CreateRecipe().AddIngredient(9).AddCondition(night);original.Register();
 var clone=original.Clone();Equal(true,clone.RemoveCondition(night));Equal(false,clone.RemoveCondition(night));clone.Register();
 Equal(true,item.RecipeDefinitions[0].EndsWith("@1"));Equal(false,item.RecipeDefinitions[1].Contains('@'));
 Reject(()=>original.RemoveCondition(night));
});
Check("recipe.clone-isolation-and-removal",()=>{
 var item=new ExampleItem();item.SetDefaults();
 var original=item.CreateRecipe(3).AddIngredient(2,10).AddNativeRecipeGroup("Wood",5).AddIngredient<ExampleItem>().AddTile(18);original.Register();
 var clone=original.Clone();Equal(true,clone.RemoveIngredient(2));Equal(false,clone.RemoveIngredient(2));
 Equal(true,clone.RemoveIngredient<ExampleItem>());clone.AddIngredient(3);clone.Register();
 Equal("3,18,v2:10,gWood:5,mExampleItem:1",item.RecipeDefinitions[0]);Equal("3,18,gWood:5,v3:1",item.RecipeDefinitions[1]);
 Reject(()=>clone.RemoveIngredient(3));Reject(()=>clone.RemoveNativeRecipeGroup("Wood"));
 var independent=clone.Clone();Equal(true,independent.RemoveNativeRecipeGroup("Wood"));Equal(false,independent.RemoveNativeRecipeGroup("Wood"));
 independent.Register();Equal("3,18,v3:1",item.RecipeDefinitions[2]);Equal("3,18,gWood:5,v3:1",item.RecipeDefinitions[1]);
 Reject(()=>independent.Clone().RemoveIngredient(0));Reject(()=>independent.Clone().RemoveIngredient("bad/key/again"));
 var empty=item.CreateRecipe().AddIngredient(2);Equal(true,empty.RemoveIngredient(2));Reject(()=>empty.Register());
});
Check("probe.items-only-full-wire",()=>{
 Directory.CreateDirectory("build/basic-items");string diag="build/basic-items/probe.txt";
 Equal(5,CompiledAssemblyProbe.Verify(Assembly.GetExecutingAssembly().Location,diag));
 var lines=File.ReadAllLines("build/basic-items/source-runtime-values.txt");Equal("BUNNY_MANAGED_DEFAULTS_V2",lines[0]);Equal(7,lines.Length);
 foreach(var line in lines.Skip(1).Where(l=>l.StartsWith("ITEM\t"))) {
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
 Check("projectile.native-tuning-real-compiler-wire-and-invalid-values",()=>{
  string root=Path.Combine(scratch,"projectile-policy");Directory.CreateDirectory(Path.Combine(root,"content"));
  string file=Path.Combine(root,"content","Policy.cs"),output=Path.Combine(scratch,"policy.dll"),diag=Path.Combine(scratch,"policy.txt");
  string source="using BunnyLoader.Source;public sealed class PolicyProjectile:BunnyProjectile{public override void SetDefaults(){DisplayName=\"Policy\";Texture=\"policy.png\";Frames=4;AnimationFrameTicks=5;MinionScanTicks=3;MinionTargetRange=950f;}}";
  File.WriteAllText(file,source);Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(1,CompiledAssemblyProbe.Verify(output,diag));
  var row=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"))[1].Split('\t');Equal(16,row.Length);Equal("950",row[15]);
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/projectile-policy-values.txt",true);
  foreach(var mutation in new[]{("AnimationFrameTicks=5","AnimationFrameTicks=0"),("MinionScanTicks=3","MinionScanTicks=121"),("MinionTargetRange=950f","MinionTargetRange=float.NaN")}) {
   File.WriteAllText(file,source.Replace(mutation.Item1,mutation.Item2));Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(-1,CompiledAssemblyProbe.Verify(output,diag));
   string error=File.ReadAllText(diag);if(!error.Contains("[CSharp][ERROR][ProjectilePolicy]")||!error.Contains("Class=PolicyProjectile")||!error.Contains("Reason=OutOfRange"))throw new Exception(error);
  }
 });
 Check("summon.native-movement-conflicting-ai-and-vanilla-style-rejected",()=>{
  string root=Path.Combine(scratch,"summon-policy");Directory.CreateDirectory(Path.Combine(root,"content"));
  string file=Path.Combine(root,"content","IndependentSummon.cs"),output=Path.Combine(scratch,"summon.dll"),diag=Path.Combine(scratch,"summon.txt");
  string source="using BunnyLoader.Source;public sealed class IndependentSummon:BunnyProjectile{public override void SetDefaults(){DisplayName=\"Summon\";Texture=\"summon.png\";int size=0;for(int i=0;i<3;++i)size+=i;Width=size*6;AiStyle=0;Minion=true;MinionSlots=1;MinionMovement=new BunnyMinionMovement();}}";
  File.WriteAllText(file,source);Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(1,CompiledAssemblyProbe.Verify(output,diag));
  var motionRow=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"))[1].Split('\t');Equal(18,motionRow.Length);Equal("18",motionRow[2]);
  foreach(var mutation in new[]{("AiStyle=0;","AiStyle=1;"),("public sealed class IndependentSummon:","public static class IndependentSummonMotion{public static float TurnAtTick(int t)=>0;} public sealed class IndependentSummon:"),("public override void SetDefaults()","public override bool AI(ref BunnyProjectileAiState state)=>true; public override void SetDefaults()")}) {
   File.WriteAllText(file,source.Replace(mutation.Item1,mutation.Item2));Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(-1,CompiledAssemblyProbe.Verify(output,diag));
   string error=File.ReadAllText(diag);if(!error.Contains("[CSharp][ERROR][ProjectilePolicy]")||!error.Contains("Class=IndependentSummon")||!error.Contains("Reason=Conflicting"))throw new Exception(error);
  }
  foreach(var create in new Action[]{()=>new BunnyMinionMovement(teleportDistance:600),()=>new BunnyMinionMovement(catchUpInertia:0),()=>new BunnyMinionMovement(ownerLeashDistance:float.NaN),()=>new BunnyMinionMovement(formationSpacing:201)})Reject(create);
 });
 Check("recipe.owned-loader-independent-source-and-callbacks",()=>{
  string output=Path.Combine(scratch,"owned.dll"),diag=Path.Combine(scratch,"owned.txt");
  Equal(1,Compiler.Compile("tests/basic_items/owned_recipe_fixture",output,runtime,diag));
  Equal(5,CompiledAssemblyProbe.Verify(output,diag));
  var rows=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));
  Equal(9,rows.Length);Equal(3,rows.Count(row=>row.StartsWith("RECIPES\t",StringComparison.Ordinal)));
  foreach(string owner in new[]{"ContractWeapon","ContractAccessory","ContractPotion"}) {
   string Call(int kind,string state)=>BunnyRecipeLoader.Dispatch("owned",output,owner,0,kind,state);
   Equal("READY",Call(3,""));Equal("1",Call(0,"10,100,0,3,9,3,0,0,0"));
   Equal("0",Call(0,"10,100,1,3,9,3,0,0,0"));Equal("0",Call(1,"10,100,0,3,9,3,0,0,0"));
   Equal("3",Call(1,"10,100,0,3,22,3,0,0,0"));Equal("OK|mContractAccessory:1",Call(2,"10,100,0,3,9,3,0,0,0"));
   Equal("READY",BunnyRecipeRuntime.Dispatch("owned",output,owner,0,3,""));
   string error=Call(2,"10,100,16,3,9,3,0,0,0");
   if(!error.Contains("Owned callback failed")||!error.Contains("Class="+owner)||!error.Contains("Stage=Invoke"))throw new Exception(error);
   Equal("READY",Call(3,""));
  }
  var rejected=BunnyRecipeLoader.Dispatch("owned",output,"ContractBuff",0,3,"");
  if(!rejected.Contains("OwnerIdentityMismatch"))throw new Exception(rejected);
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/owned-source-runtime-values.txt",true);
 });
 Check("recipe.managed-callbacks-real-source",()=>{
  string output=Path.Combine(scratch,"callbacks.dll"),diag=Path.Combine(scratch,"callbacks.txt");
  Equal(1,Compiler.Compile("tests/basic_items/callback_fixture",output,runtime,diag));Equal(1,CompiledAssemblyProbe.Verify(output,diag));
  var line=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"))[1];
  if(!line.Split('\t')[19].EndsWith(":3@7"))throw new Exception("Missing callback registration mask");
  string Call(int kind,string state)=>BunnyRecipeRuntime.Dispatch("contract",output,"ContractRecipeItem",0,kind,state);
  Equal("READY",Call(3,""));
  Equal("1",Call(0,"10,100,0,3,9,3,0,0,0"));Equal("0",Call(0,"90,100,0,3,9,3,0,0,0"));
  Equal("0",Call(0,"10,100,1,3,9,3,0,0,0"));Equal("0",Call(1,"10,100,0,3,9,3,0,0,0"));
  Equal("3",Call(1,"10,100,0,3,22,3,0,0,0"));Equal("OK|mContractRecipeItem:2",Call(2,"10,100,0,3,9,3,0,0,0"));
  var error=Call(2,"10,100,16,3,9,3,0,0,0");
  if(!error.Contains("Fixture callback failure")||!error.Contains("ContractRecipeItem")||!error.Contains("Stage=Invoke"))throw new Exception(error);
  var invalid=BunnyRecipeRuntime.Dispatch("contract",output,"ContractRecipeItem",99,0,"10,100,0,3,9,3,0,0,0");
  if(!invalid.StartsWith("ERROR|"))throw new Exception("Unregistered handler accepted");
 });
 Check("recipe.class-hooks-isolation-and-stage",()=>{
  string output=Path.Combine(scratch,"behavior.dll"),diag=Path.Combine(scratch,"behavior.txt");
  Equal(1,Compiler.Compile("tests/basic_items/recipe_behavior_fixture",output,runtime,diag));
  string Call(string mod,string owner,int index,int kind,string state)=>BunnyRecipeRuntime.Dispatch(mod,output,owner,index,kind,state);
  const string state="10,100,4,3,9,2,10,0,0";
  Equal("READY",Call("behavior","BehaviorItem",0,3,""));
  Equal("1",Call("behavior","BehaviorItem",0,0,state));
  Equal("0",Call("behavior","BehaviorItem",0,0,"10,100,5,3,9,2,10,0,0"));
  Equal("0",Call("behavior","BehaviorItem",0,0,"10,100,0,3,9,2,10,0,0"));
  Equal("0",Call("behavior","BehaviorItem",0,1,state));
  Equal("2",Call("behavior","BehaviorItem",0,1,"10,100,4,3,22,2,10,0,0"));
  Equal("OK|mBehaviorItem:1",Call("behavior","BehaviorItem",0,2,state));
  Equal("OK|mBehaviorItem:1;mBehaviorItem:2",Call("behavior","BehaviorItem",1,2,state));
  Equal("1",Call("behavior","BehaviorItem",4,0,"10,100,0,3,9,2,10,0,0"));
  Equal("0",Call("behavior","BehaviorItem",4,0,"10,100,1,3,9,2,10,0,0"));
  Equal("0",Call("behavior","BehaviorItem",4,0,state));
  var broken=Call("behavior-first-broken","UnrelatedBrokenItem",0,3,"");
  if(!broken.Contains("UnrelatedOwnerFailure"))throw new Exception(broken);
  Equal("READY",Call("behavior-first-broken","BehaviorItem",0,3,""));
  foreach(var c in new[]{(2,0),(3,1)}){
   var error=Call("behavior","BehaviorItem",c.Item1,c.Item2,state);
   if(!error.Contains("OperationNotAllowed")||!error.Contains("GiveItem"))throw new Exception(error);
  }
  foreach(string bad in new[]{"10,100,4,0,9,2,10,0,0","10,100,4,3,9,2,-1,0,0","10,100,4,3,9,2,10,1,0"})
   if(!Call("behavior","BehaviorItem",0,0,bad).Contains("Snapshot outside native contract"))throw new Exception("Invalid snapshot accepted");
 });
 Check("buff.managed-flags-calculated-defaults",()=>{
  string output=Path.Combine(scratch,"buffs.dll"),diag=Path.Combine(scratch,"buffs.txt");
  Equal(1,Compiler.Compile("tests/basic_items/buff_fixture",output,runtime,diag));Equal(2,CompiledAssemblyProbe.Verify(output,diag));
  string line=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt")).Single(s=>s.StartsWith("BUFF\t"));
  Equal("BUFF\tComputedBuff\t10\tContractProjectile\t15\t43616C63756C6174656420626F6E75733A203130\t100",line);
 });
 Check("recipe.groups-real-source-wire",()=>{
  string root="tests/basic_items/recipe_group_fixture",output=Path.Combine(scratch,"groups.dll"),diag=Path.Combine(scratch,"groups.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(1,CompiledAssemblyProbe.Verify(output,diag));
  var lines=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));Equal(2,lines.Length);
  Equal("1,mBench,gWood:10,gIronBar:2,mContractGroupedBench:3",lines[1].Split('\t')[19]);
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/group-source-runtime-values.txt",true);
 });
 Check("roslyn.auto-items-only-package",()=>{
  string mod=Path.Combine(scratch,"mod");Directory.CreateDirectory(Path.Combine(mod,"content"));
  foreach(string path in new[]{"Content/Items/ExampleItem.cs","Content/Items/Tools/ExamplePickaxe.cs","Content/Items/Tools/ExampleHamaxe.cs","Content/Rarities/ExampleModRarity.cs","Content/Rarities/ExampleHigherTierModRarity.cs"})
   File.Copy(Path.Combine("samples/ExampleModCSharp/Playable/content",path),Path.Combine(mod,"content",Path.GetFileName(path)));
  string output=Path.Combine(scratch,"items.dll"),diag=Path.Combine(scratch,"compile.txt");
  Equal(1,Compiler.Compile(mod,output,runtime,diag));Equal(5,CompiledAssemblyProbe.Verify(output,diag));Equal(2,Compiler.Compile(mod,output,runtime,diag));
 });
 Check("roslyn.tile-animation-native-wire",()=>{
  string folder=Path.Combine(scratch,"animated-tile");Directory.CreateDirectory(Path.Combine(folder,"content"));
  File.WriteAllText(Path.Combine(folder,"content","AnimatedExampleTile.cs"),"using BunnyLoader.Source;public sealed class AnimatedExampleTile:BunnyTile{public override void SetStaticDefaults(){Texture=\"Content/Tiles/AnimatedExampleTile.png\";DisplayName=\"Example Animated Tile\";AnimationFrameHeight=36;AnimationFrames=4;AnimationFrameTicks=5;AdjacentTiles=new[]{18,16};AllowBreaking=false;AllowDrops=false;CacheDrawData=false;}public override bool PreDraw(int x,int y)=>x!=3;public override bool CanKillTile(int x,int y){if(x==99)throw new System.InvalidOperationException(\"Independent tile error\");return false;}public override bool CanDrop(int x,int y)=>x<3;public override void HitWire(int x,int y){}public override void RandomUpdate(int x,int y){}public override void AnimateTile(ref int frame,ref int counter){frame=(frame+1)%4;counter=0;}public override void ModifyLight(int x,int y,ref float r,ref float g,ref float b){r=0.2f;g=0.4f;b=0.8f;}public override void NearbyEffects(int x,int y,bool closer){}public override void SetDrawPositions(int x,int y,ref int width,ref int top,ref int height,ref int frameX,ref int frameY){top=-4;frameX+=18;}public override void AnimateIndividualTile(int x,int y,ref int addFrameX,ref int addFrameY){addFrameY+=36;}public override void SetSpriteEffects(int x,int y,ref int effects){effects=1;} }");
  string assemblyPath=Path.Combine(scratch,"animated-tile.dll"),diagPath=Path.Combine(scratch,"animated-tile.txt");
  Equal(1,Compiler.Compile(folder,assemblyPath,runtime,diagPath));
  Equal(1,CompiledAssemblyProbe.Verify(assemblyPath,diagPath));
  var tileWire=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"))
   .Single(line=>line.StartsWith("TILE\tAnimatedExampleTile\t",StringComparison.Ordinal)).Split('\t');
  Equal(21,tileWire.Length);Equal("36",tileWire[13]);Equal("4",tileWire[14]);Equal("5",tileWire[15]);Equal("16,18",tileWire[16]);Equal("0",tileWire[17]);Equal("0",tileWire[18]);Equal("63599",tileWire[19]);Equal("0",tileWire[20]);
  Equal("READY",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",0,2,""));
  Equal("0",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",0,0,"3,4"));
  Equal("1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",1,0,"2,4"));
  Equal("0",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",1,0,"5,4"));
  Equal("1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",2,0,"4,4"));
  Equal("1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",3,0,"4,4"));
  Equal("0,0",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",12,0,"3,4"));
  Equal("0",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",5,0,"3,4"));
  Equal("1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",5,0,"4,4"));
  Equal("LIGHT|200|400|800",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",6,0,"3,4"));
  Equal("1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",11,0,"3,4,0"));
  Equal("1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",11,0,"3,4,1"));
  Equal("DRAW|18|0|16|16|-4|0|36|1|0",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,
   "AnimatedExampleTile",13,0,"3,4,0,0,16,16,0,0,0,0,0"));
  Equal("DRAW|18|0|16|16|-4|0|36|1|1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,
   "AnimatedExampleTile",13,0,"4,4,0,0,16,16,0,0,0,0,0"));
  Equal("DRAW|18|0|16|16|-4|0|36|1",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,
   "AnimatedExampleTile",13,0,"3,4,0,0,16,16,0,0,0,0,1"));
  if(!BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",13,0,
    "3,4,0,0,16,16,0,0,0,0,2").StartsWith("ERROR|Stage=DecodeTileDraw",StringComparison.Ordinal))
    throw new Exception("Invalid draw pass accepted");
  var callbackError=BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",0,0,"99,4");
  foreach(string detail in new[]{"Mod=tile-fixture","Class=AnimatedExampleTile","Callback=CanKillTile","Stage=Invoke","System.InvalidOperationException","Independent tile error","CanKillTile("})
   if(!callbackError.Contains(detail))throw new Exception(callbackError);
  Equal("0",BunnyTileRuntime.Dispatch("tile-fixture",assemblyPath,"AnimatedExampleTile",0,0,"3,4"));
 });
 Check("wall.real-compiler-policy-and-native-helper-overrides",()=>{
  string folder=Path.Combine(scratch,"wall"),file=Path.Combine(folder,"content","Wall.cs");Directory.CreateDirectory(Path.GetDirectoryName(file)!);
  File.WriteAllText(file,"using BunnyLoader.Source;namespace Walls {public sealed class IndependentWall:BunnyWall {public override void SetStaticDefaults(){Texture=\"wall.png\";DisplayName=\"Example Independent Wall\";Housing=true;BlendWallName=\"CogWall\";DustName=\"Stone\";FailedDustCount=3;DustCount=10;AnimationFrames=2;AnimationFrameTicks=5;SetFrameDust(0,\"GemEmerald\");RandomizeFrame(0,.75f,1,2);NightLight=new(.1f,.5f,0);AddMapEntry(68,68,68,\"Example Wall\");}}} namespace Items {public sealed class IndependentWall:BunnyItem {public override void SetDefaults(){Texture=\"item.png\";DisplayName=\"Example Independent Wall\";DefaultToPlaceableWall(\"IndependentWall\");Width=0;UseTime=7;AutoReuse=false;}public override void AddRecipes(){CreateRecipe(4).AddIngredient(\"OtherMod/Material\").AddTile(\"OtherMod/Station\").Register();}}}");
  string output=Path.Combine(folder,"wall.dll"),diag=Path.Combine(folder,"wall.txt");
  Equal(1,Compiler.Compile(folder,output,runtime,diag));if(CompiledAssemblyProbe.Verify(output,diag)!=2)throw new Exception(File.ReadAllText(diag));
  var rows=File.ReadAllLines(Path.Combine(folder,"source-runtime-values.txt"));
  var wall=rows.Single(r=>r.StartsWith("WALL\t")).Split('\t');Equal(16,wall.Length);Equal("3",wall[5]);Equal("10",wall[6]);Equal("2",wall[7]);Equal("5",wall[8]);Equal("0.75",wall[10]);Equal("1,2",wall[11]);Equal("0:GemEmerald",wall[12]);Equal("0.1,0.5,0",wall[14]);
  var item=rows.Single(r=>r.StartsWith("ITEM\t")).Split('\t');Equal("wmIndependentWall:0:129:4",item[21]);Equal("4,mOtherMod/Station,mOtherMod/Material:1",item[19]);
  File.Copy(Path.Combine(folder,"source-runtime-values.txt"),"build/basic-items/wall-source-runtime-values.txt",true);
  Reject(()=>new BunnyWorldLight(float.NaN,0,0));Reject(()=>new BunnyWorldLight(0,-1,0));
 });
 Check("roslyn.full-source-package",()=>{
  string root="samples/ExampleModCSharp/Playable",output=Path.Combine(scratch,"all.dll"),diag=Path.Combine(scratch,"all.txt");
  Equal(1,Compiler.Compile(root,output,runtime,diag));
  var domains=new Dictionary<Type,string>{
   [typeof(BunnyItem)]="ITEM",[typeof(BunnyWeapon)]="ITEM",[typeof(BunnyAccessory)]="ITEM",[typeof(BunnyBuffPotion)]="ITEM",
   [typeof(BunnyWall)]="WALL",[typeof(BunnyTile)]="TILE",[typeof(BunnyProjectile)]="PROJECTILE",[typeof(BunnyNPC)]="NPC",
   [typeof(BunnyRarity)]="RARITY",[typeof(BunnyDust)]="DUST",[typeof(BunnyBuff)]="BUFF",[typeof(BunnyPlayer)]="PLAYER",[typeof(BunnyPrefix)]="PREFIX"
  };
  var compiled=Assembly.Load(File.ReadAllBytes(output));
  var identities=compiled.GetTypes().Where(t=>t.IsClass&&!t.IsAbstract&&!t.ContainsGenericParameters)
   .Select(t=>new{type=t,domain=domains.FirstOrDefault(p=>p.Key.IsAssignableFrom(t)).Value})
   .Where(x=>x.domain is not null).Select(x=>x.domain+":"+x.type.Name).ToHashSet(StringComparer.Ordinal);
  if(identities.Count==0)throw new Exception("full package has no content");
  int localizationModules=compiled.GetTypes().Count(t=>t.IsClass&&!t.IsAbstract&&
   typeof(BunnyLocalizationModule).IsAssignableFrom(t));
  int commands=compiled.GetTypes().Count(t=>t.IsClass&&!t.IsAbstract&&
   typeof(BunnyCommand).IsAssignableFrom(t));
  Equal(3,commands);
  if(CompiledAssemblyProbe.Verify(output,diag,root)!=identities.Count+localizationModules+commands)
   throw new Exception(File.ReadAllText(diag));
  var contentKinds=new HashSet<string>{"ITEM","WEAPON","ACCESSORY","POTION","TILE","WALL","DUST","RARITY","BUFF","PLAYER","PROJECTILE","NPC","PREFIX"};
  string localizationUid="68c3115f-62f4-49f2-a56b-388256781d5f";
  string dynamicWire=BunnyLocalizationRuntime.Dispatch(localizationUid,output,"pt-BR");
  if(!dynamicWire.StartsWith("OK|",StringComparison.Ordinal)||
     !dynamicWire.Contains(Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes(
       "Mods."+localizationUid+".Common.Localization.RuntimeGreeting")),StringComparison.Ordinal))
    throw new Exception("Actual Example localization callback failed with registered UID: "+dynamicWire);
  Equal(true,BunnyLocalizationRuntime.Unload(localizationUid));
  var actual=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt")).Skip(1)
   .Where(l=>l.Length>0).Select(l=>l.Split('\t')).Where(f=>contentKinds.Contains(f[0])).Select(f=>
    (f[0] is "WEAPON" or "ACCESSORY" or "POTION"?"ITEM":f[0])+":"+f[1]).ToArray();
  Equal(identities.Count,actual.Length);
  if(!identities.SetEquals(actual))throw new Exception("compiled types and native wire identities differ");
  File.Copy(Path.Combine(scratch,"source-runtime-values.txt"),"build/basic-items/full-source-runtime-values.txt",true);
  var minionRows=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));
  // The native bootstrap defers SDK calls; verify the real managed defaults still apply every rarity.
  foreach(var pair in new[]{("ExampleItem","ExampleModRarity"),("ExampleMeleeWeapon","ExampleModRarity"),
      ("ExampleStatAccessory","ExampleHigherTierModRarity"),("ExampleWand","ExampleModRarity"),
      ("ExampleBuffPotion","ExampleHigherTierModRarity")})
    Equal(true,minionRows.Contains("ITEM_RARITY\t"+pair.Item1+"\t"+pair.Item2));
  var blockRows=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt"));
  var blockTile=blockRows.Single(row=>row.StartsWith("TILE\tExampleTile\t",StringComparison.Ordinal)).Split('\t');
  Equal("19968",blockTile[2]);Equal("0",blockTile[3]);Equal("Stone",blockTile[6]);
  Equal("0",blockTile[7]);Equal("200",blockTile[8]);Equal("255",blockTile[9]);
  Equal(21,blockTile.Length);Equal("0",blockTile[13]);Equal("1",blockTile[14]);Equal("0",blockTile[15]);Equal("-",blockTile[16]);Equal("1",blockTile[17]);Equal("1",blockTile[18]);Equal("0",blockTile[19]);Equal("1",blockTile[20]);
  var blockItem=blockRows.Single(row=>row.StartsWith("ITEM\tExampleTileItem\t",StringComparison.Ordinal)).Split('\t');
  Equal("12",blockItem[2]);Equal("12",blockItem[3]);
  Equal("10,18,mExampleItem:1",blockItem[19]);Equal("mExampleTile:0",blockItem[21]);

  var minion=minionRows.Single(row=>row.StartsWith("PROJECTILE\tExampleMinion\t",StringComparison.Ordinal)).Split('\t');
  Equal(18,minion.Length);Equal("4",minion[12]);Equal("5",minion[13]);Equal("8",minion[14]);Equal("700",minion[15]);Equal("1",minion[16]);Equal("4,8,12,80,20,10,-48,600,20,0.82,60,2000,40,40,1200,0.05,0.04",minion[17]);
  Equal(true,minionRows.Contains("RECIPES\tExampleMinionItem\tWEAPON\t1,mExampleWorkbench,mExampleItem:1"));
  var sword=minionRows.Single(row=>row.StartsWith("ITEM\tExampleMeleeWeapon\t",StringComparison.Ordinal)).Split('\t');Equal("50000",sword[5]);
  var bulletItem=minionRows.Single(row=>row.StartsWith("ITEM\tExampleBullet\t",StringComparison.Ordinal)).Split('\t');Equal("5000",bulletItem[5]);
  var bullet=minionRows.Single(row=>row.StartsWith("NATIVE_ITEM\tExampleBullet\t",StringComparison.Ordinal));
  Equal(true,bullet.Contains("ammo:d:AmmoID:Bullet")&&bullet.Contains("shoot:p:ExampleBulletProjectile")&&bullet.Contains("shootSpeed:f:4.5"));
  var projectile=minionRows.Single(row=>row.StartsWith("NATIVE_PROJECTILE\tExampleBulletProjectile\t",StringComparison.Ordinal));
  Equal(true,projectile.Contains("localNPCHitCooldown:i:-1")&&projectile.Contains("extraUpdates:i:1")&&projectile.Contains("light:f:0.5"));
  foreach(var invalid in new[]{0,121}) {
   Reject(()=>new BunnyMinionMovement(attackSpeed:invalid));
  }
  var benchRecipes=File.ReadAllLines(Path.Combine(scratch,"source-runtime-values.txt")).Single(row=>row.StartsWith("ITEM\tExampleWorkbench\t",StringComparison.Ordinal)).Split('\t')[19];
  Equal("1,-1,v36:1,mExampleItem:10;1,-1,gWood:10,mExampleItem:10;1,-1,mExampleItem:10,gIronBar:2;1,-1,hExampleMaterials~416E79204578616D706C65206D6174657269616C~mExampleItem+mExampleSoul:10,gWood:10@7",benchRecipes);
  var assets=new List<object>();
  foreach(var type in compiled.GetTypes().Where(t=>t.IsClass&&!t.IsAbstract&&!t.ContainsGenericParameters&&
      domains.Keys.Any(baseType=>baseType.IsAssignableFrom(t)))) {
    var instance=Activator.CreateInstance(type)??throw new Exception("Cannot instantiate "+type.FullName);
    var defaults=type.GetMethod((typeof(BunnyTile).IsAssignableFrom(type)||typeof(BunnyWall).IsAssignableFrom(type)||typeof(BunnyDust).IsAssignableFrom(type))?"SetStaticDefaults":"SetDefaults");
    defaults?.Invoke(instance,null);
    // Execute every registered content class: names and localized names share the user-facing convention.
    if(type.GetProperty("DisplayName")?.GetValue(instance) is string displayName &&
       !displayName.StartsWith("Example ",StringComparison.Ordinal))
      throw new Exception("Content name must begin with Example: "+type.FullName+" = "+displayName);
    foreach(var name in type.GetCustomAttributes<BunnyNameAttribute>())
      if(!name.Text.StartsWith("Example ",StringComparison.Ordinal))
        throw new Exception("Localized name must begin with Example: "+type.FullName+" = "+name.Text);

    void RequireAsset(string? name) {
      if(!string.IsNullOrWhiteSpace(name))
        assets.Add(new {path=name,className=type.Name,file="ExecutedCSharpDefaults"});
    }
    if(instance is BunnyDust dust) {
      var state=new BunnyDustState{Slot=0,Type=332,Generation=1,Active=1,Scale=1,FrameWidth=8,FrameHeight=8,FrameY=20};
      dust.OnSpawn(ref state);
      byte[] png=File.ReadAllBytes(Path.Combine(root,"content",dust.Texture));
      int textureWidth=System.Buffers.Binary.BinaryPrimitives.ReadInt32BigEndian(png.AsSpan(16,4));
      int textureHeight=System.Buffers.Binary.BinaryPrimitives.ReadInt32BigEndian(png.AsSpan(20,4));
      if(state.FrameX<0||state.FrameY<0||state.FrameWidth<=0||state.FrameHeight<=0||
         state.FrameX+state.FrameWidth>textureWidth||state.FrameY+state.FrameHeight>textureHeight)
        throw new Exception("Dust OnSpawn frame exceeds original PNG: "+type.Name);
    }
    foreach(var propertyName in new[]{"Texture","VisualTexture"})
      RequireAsset(type.GetProperty(propertyName)?.GetValue(instance) as string);
    if(type.GetProperty("CustomUseSound")?.GetValue(instance) is BunnySoundStyle sound)RequireAsset(sound.Path);
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
 Check("tiles.unknown-flags-rejected-by-real-compiler-probe",()=>{
  string root=Path.Combine(scratch,"bad-tile-flags");Directory.CreateDirectory(Path.Combine(root,"content"));
  string output=Path.Combine(scratch,"bad-tile.dll"),diag=Path.Combine(scratch,"bad-tile.txt");
  foreach(int flags in new[]{-1,65536,int.MaxValue}) {
   File.WriteAllText(Path.Combine(root,"content","InvalidTile.cs"),
    "using BunnyLoader.Source;public sealed class InvalidTile:BunnyTile{public override void SetStaticDefaults(){Texture=\"InvalidTile.png\";DisplayName=\"Example Invalid\";Flags=(BunnyTileFlags)("+flags+");}}");
   Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(-1,CompiledAssemblyProbe.Verify(output,diag));
   string error=File.ReadAllText(diag);
   foreach(string detail in new[]{"Class=InvalidTile","Field=Flags","Stage=Serialize","Expected=Int32:KnownMask0xFFFF","Actual="+flags,"Reason=UnknownFlagBits"})
    if(!error.Contains(detail))throw new Exception("Missing tile diagnostic: "+detail+" in "+error);
  }
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
  Equal(21,tile.Length);Equal("1",tile[20]); Equal("1",tile[3]);Equal("18",tile[4]);Equal("18",tile[5]);
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

sealed class EmptyBehavior : BunnyRecipeBehavior { }
sealed class HiddenBehavior : BunnyRecipeBehavior
{
 public new bool IsAvailable(BunnyCraftContext context)=>false;
}
sealed class FullBehavior : BunnyRecipeBehavior
{
 public override bool IsAvailable(BunnyCraftContext context)=>true;
 public override int ConsumeIngredient(BunnyCraftContext context,int type,int amount)=>amount;
 public override void OnCraft(BunnyCraftContext context) { }
}
