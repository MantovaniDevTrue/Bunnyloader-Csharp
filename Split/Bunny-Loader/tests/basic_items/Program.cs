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
Check("recipe.reject-invalid",()=>{var item=new ExampleItem();item.SetDefaults();Reject(()=>item.CreateRecipe(0));Reject(()=>item.CreateRecipe().AddIngredient(0));Reject(()=>item.CreateRecipe().AddIngredient("bad,name"));Reject(()=>item.CreateRecipe().AddTile(2195));Reject(()=>item.CreateRecipe().Register());Reject(()=>item.CreateRecipe().AddTile(18).AddTile(16));});
Check("recipe.once-and-limits",()=>{var item=new ExampleItem();item.SetDefaults();var r=item.CreateRecipe().AddIngredient(2);r.Register();Reject(()=>r.Register());Reject(()=>r.AddIngredient(3));var many=item.CreateRecipe();for(int i=0;i<15;i++)many.AddIngredient(2);Reject(()=>many.AddIngredient(2));});
Check("probe.items-only-full-wire",()=>{
 Directory.CreateDirectory("build/basic-items");string diag="build/basic-items/probe.txt";
 Equal(3,CompiledAssemblyProbe.Verify(Assembly.GetExecutingAssembly().Location,diag));
 var lines=File.ReadAllLines("build/basic-items/source-runtime-values.txt");Equal("BUNNY_MANAGED_DEFAULTS_V2",lines[0]);Equal(4,lines.Length);
 foreach(var line in lines.Skip(1))Equal(20,line.Split('\t').Length);
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
 Check("roslyn.full-45-source-package",()=>{
  string root="samples/ExampleModCSharp/Playable",output=Path.Combine(scratch,"all.dll"),diag=Path.Combine(scratch,"all.txt");
  Equal(45,Directory.GetFiles(root,"*.cs",SearchOption.AllDirectories).Length);Equal(1,Compiler.Compile(root,output,runtime,diag));Equal(31,CompiledAssemblyProbe.Verify(output,diag));
  if(new FileInfo(Path.Combine(scratch,"source-runtime-values.txt")).Length>8192)throw new Exception("wire exceeds runtime budget");
 });
} finally {if(Directory.Exists(scratch))Directory.Delete(scratch,true);}
Directory.CreateDirectory("build");string json=JsonSerializer.Serialize(new{schemaVersion=1,failures,results},new JsonSerializerOptions{WriteIndented=true});File.WriteAllText("build/basic-items-results.json",json);Console.WriteLine(json);return failures==0?0:1;
