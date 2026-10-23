using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>Snapshot copied on the Terraria thread; no live native pointers escape to MonoVM.</summary>
public sealed class BunnyCraftContext
{
    internal BunnyCraftContext() { }
    public int CurrentLife { get; internal set; }
    public int MaxLife { get; internal set; }
    public bool IsDay { get; internal set; }
    public bool InDesert { get; internal set; }
    public bool NearWater { get; internal set; }
    public bool InSnow { get; internal set; }
    public bool QuickCraft { get; internal set; }
    public int ResultType { get; internal set; }
    public int AvailableIngredientCount { get; internal set; }
    public BunnyRecipeStage Stage { get; internal set; }
    private List<string>? grants;
    internal string Grants => grants==null?"":string.Join(";",grants);
    /// <summary>Request a native item grant after this callback succeeds; maximum eight commands.</summary>
    public void GiveItem(int vanillaItemId,int count=1)
    {
        if(vanillaItemId<1||vanillaItemId>=6196)throw new ArgumentOutOfRangeException(nameof(vanillaItemId));
        Give("v"+vanillaItemId.ToString(CultureInfo.InvariantCulture),count);
    }
    public void GiveItem(string modItemName,int count=1)
    {
        BunnyContentKey.Validate(modItemName,nameof(modItemName),"BunnyCraftContext.GiveItem");
        Give("m"+modItemName,count);
    }
    public void GiveItem<TItem>(int count=1) where TItem:class
        => GiveItem(BunnyContentKey.TypeName(typeof(TItem),false,"BunnyCraftContext.GiveItem<TItem>"),count);
    private void Give(string key,int count)
    {
        if(Stage!=BunnyRecipeStage.Craft)throw new InvalidOperationException("[CSharp][ERROR][RecipeCallback] API=BunnyCraftContext.GiveItem Stage="+Stage+" Expected=Craft Reason=OperationNotAllowed");
        if(count<1||count>9999||(grants?.Count??0)>=8)throw new ArgumentOutOfRangeException(nameof(count));
        (grants??=new()).Add(key+":"+count.ToString(CultureInfo.InvariantCulture));
    }
}

/// <summary>Internal transport endpoint, public for Mono reflection. All callbacks execute on the existing Mono worker.</summary>
public static class BunnyRecipeLoader
{
    private sealed class Mod
    {
        public string Path="";
        public Assembly Assembly=null!;
        public AssemblyLoadContext Context=null!;
        public Dictionary<string,List<BunnyRecipe>> Recipes=new(StringComparer.Ordinal);
    }

    private sealed class ModContext : AssemblyLoadContext
    {
        internal ModContext(string modId) : base("BunnyMod:"+modId,isCollectible:true) { }

        protected override Assembly? Load(AssemblyName name)
        {
            Assembly shared=typeof(BunnyRecipeLoader).Assembly;
            if(name.Name==shared.GetName().Name)return shared;
            return Default.Assemblies.FirstOrDefault(candidate=>
                string.Equals(candidate.GetName().Name,name.Name,StringComparison.Ordinal));
        }
    }

    private static void Evict(string modId,Mod mod)
    {
        mods.Remove(modId);
        BunnyQuestRuntime.EvictMod(modId);
        BunnyAchievementWorker.Evict(modId, mod.Path);
        mod.Recipes.Clear();
        mod.Context.Unload();
    }
    private static readonly Dictionary<string,Mod> mods=new(StringComparer.Ordinal);
    [ThreadStatic] private static List<BunnyRecipe>? capture;
    internal static void Capture(BunnyRecipe recipe) { capture?.Add(recipe); }
    internal static string AssemblyFilePath(string assemblyPath)
    {
        const string marker="|bunny-revision=";
        int suffix=assemblyPath.LastIndexOf(marker,StringComparison.Ordinal);
        string filePath=assemblyPath;
        if(suffix>=0)
        {
            string revision=assemblyPath[(suffix+marker.Length)..];
            if(revision.Length<1||revision.Length>20||
                revision.Any(ch=>ch<'0'||ch>'9'))
                throw new InvalidOperationException("Invalid managed assembly revision");
            filePath=assemblyPath[..suffix];
        }
        return filePath;
    }
    internal static Assembly LoadAssembly(string modId,string assemblyPath)
    {
        string filePath=AssemblyFilePath(assemblyPath);
        if(!mods.TryGetValue(modId,out Mod? mod)||mod.Path!=assemblyPath)
        {
            if(mod!=null)Evict(modId,mod);
            var context=new ModContext(modId);
            try
            {
                using var bytes=new MemoryStream(File.ReadAllBytes(filePath),writable:false);
                var loaded=context.LoadFromStream(bytes);
                mod=new Mod{Path=assemblyPath,Assembly=loaded,Context=context};
                mods[modId]=mod;
            }
            catch
            {
                context.Unload();
                throw;
            }
        }
        return mod.Assembly;
    }
    public static string Dispatch(string modId,string assemblyPath,string owner,int index,int kind,string snapshot)
    {
        try
        {
            if(owner=="__achievements__" && index==0 && kind>=0 && kind<=4)
                return BunnyAchievementWorker.Dispatch(modId,assemblyPath,kind,snapshot);
            if(kind==6)
            {
                if(index!=0||owner!="__unload__"||snapshot.Length!=0)
                    throw new ArgumentException("Invalid managed unload request");
                if(!mods.TryGetValue(modId,out Mod? previous)||previous.Path!=assemblyPath)
                    return "PENDING|Stage=Unload Reason=BindingNotFoundOrReplaced";
                Evict(modId,previous);
                return "CLEARED";
            }
            if(kind==4||kind==5)
                return BunnyQuestRuntime.Dispatch(modId,assemblyPath,owner,kind,snapshot);
            LoadAssembly(modId,assemblyPath);
            Mod mod=mods[modId];
            if(!mod.Recipes.TryGetValue(owner,out var recipes))
            {
                    var candidates=mod.Assembly.GetTypes().Where(t=>t.Name==owner&&t.IsClass&&!t.IsAbstract&&!t.ContainsGenericParameters&&typeof(BunnyRecipeSource).IsAssignableFrom(t)).ToArray();
                    if(candidates.Length!=1)throw new InvalidOperationException("[CSharp][ERROR][RecipeLoader] Class="+owner+" Stage=ResolveOwner Expected=OneRegisteredItemCategory Actual="+candidates.Length+" Reason=OwnerIdentityMismatch");
                    Type type=candidates[0];
                    BunnyContentKind ownerKind=typeof(BunnyItem).IsAssignableFrom(type)?BunnyContentKind.Item:
                        typeof(BunnyWeapon).IsAssignableFrom(type)?BunnyContentKind.Weapon:
                        typeof(BunnyAccessory).IsAssignableFrom(type)?BunnyContentKind.Accessory:
                        typeof(BunnyBuffPotion).IsAssignableFrom(type)?BunnyContentKind.BuffPotion:
                        throw new InvalidOperationException("Unsupported recipe source category: "+type.FullName);
                    var item=(BunnyRecipeSource)Activator.CreateInstance(type)!;
                    var declaration=type.GetCustomAttribute<BunnyContentAttribute>(false);
                    if(declaration!=null){
                        if(declaration.Kind!=ownerKind)throw new InvalidOperationException("Declared recipe owner must be BunnyContentKind."+ownerKind+": "+type.FullName);
                        type.GetProperty("DisplayName")!.SetValue(item,declaration.DisplayName);
                        type.GetProperty("Texture")!.SetValue(item,declaration.Texture);
                    }
                    type.GetMethod("SetDefaults",Type.EmptyTypes)!.Invoke(item,null);
                    if(declaration!=null&&((string?)type.GetProperty("DisplayName")!.GetValue(item)!=declaration.DisplayName||(string?)type.GetProperty("Texture")!.GetValue(item)!=declaration.Texture))
                        throw new InvalidOperationException("SetDefaults changed declared recipe-owner identity: "+type.FullName);
                    var previous=capture;capture=new List<BunnyRecipe>();
                    try { item.AddRecipes();recipes=capture;mod.Recipes.Add(type.Name,recipes); }
                    finally { capture=previous; }
            }
            if((uint)index>=(uint)recipes.Count)
                throw new InvalidOperationException("Recipe handler identity not registered");
            if(kind==3)return "READY"; // Bind and reconstruct handlers before the native recipe is acknowledged.
            if(snapshot.Length>512)throw new ArgumentException("Snapshot exceeds 512 characters");
            Span<int> n=stackalloc int[9];ReadOnlySpan<char> remaining=snapshot.AsSpan();
            for(int field=0;field<9;field++)
            {
                int comma=remaining.IndexOf(',');
                if((field==8&&comma>=0)||(field<8&&comma<0))throw new ArgumentException("Expected nine snapshot fields");
                var text=comma<0?remaining:remaining[..comma];
                if(!int.TryParse(text,NumberStyles.Integer,CultureInfo.InvariantCulture,out n[field]))
                    throw new ArgumentException("Invalid Int32 snapshot field="+field+" Actual="+text.ToString());
                remaining=comma<0?ReadOnlySpan<char>.Empty:remaining[(comma+1)..];
            }
            if(n[0]<0||n[1]<1||n[2]<0||n[2]>31||n[3]<1||n[4]<0||n[5]<0||n[5]>9999||n[6]<0||n[7]!=0||n[8]!=0)
                throw new ArgumentException("Snapshot outside native contract");
            var context=new BunnyCraftContext{CurrentLife=n[0],MaxLife=n[1],IsDay=(n[2]&1)!=0,
                InDesert=(n[2]&2)!=0,NearWater=(n[2]&4)!=0,InSnow=(n[2]&8)!=0,
                QuickCraft=(n[2]&16)!=0,ResultType=n[3],AvailableIngredientCount=n[6],Stage=(BunnyRecipeStage)kind};
            BunnyRecipe recipe=recipes[index];
            if(kind==0)return recipe.IsAvailable(context)?"1":"0";
            if(kind==1)return recipe.Consume(context,n[4],n[5]).ToString(CultureInfo.InvariantCulture);
            if(kind==2){recipe.Crafted(context);return "OK|"+context.Grants;}
            throw new ArgumentOutOfRangeException(nameof(kind));
        }
        catch(Exception error)
        {
            return "ERROR|[CSharp][ERROR][RecipeCallback] Mod="+modId+" Class="+owner+" File="+assemblyPath+
                " RecipeIndex="+index+" Stage=Invoke Kind="+kind+" Reason=ManagedException Result=Rejected Exception="+error;
        }
    }
}

public static class BunnyQuestRuntime
{
    private sealed class Handler
    {
        public string Path="";
        public BunnyItem Item=null!;
    }
    private static readonly Dictionary<string,Handler> handlers=new(StringComparer.Ordinal);

    internal static void EvictMod(string modId)
    {
        string prefix=modId+"\n";
        foreach(string key in handlers.Keys.Where(key=>key.StartsWith(prefix,StringComparison.Ordinal)).ToArray())
            handlers.Remove(key);
    }

    public static string Dispatch(string modId,string assemblyPath,string owner,int kind,string snapshot)
    {
        try
        {
            if(kind!=4&&kind!=5)throw new ArgumentOutOfRangeException(nameof(kind));
            if(snapshot.Length>128)throw new ArgumentException("Quest snapshot too large");
            string[] parts=snapshot.Split(',');
            if(kind==4?parts.Length!=2:parts.Length!=3&&parts.Length!=4)
                throw new ArgumentException("Invalid quest snapshot width");
            var numbers=new int[parts.Length];
            for(int i=0;i<parts.Length;++i)
                if(!int.TryParse(parts[i],NumberStyles.None,CultureInfo.InvariantCulture,out numbers[i]))
                    throw new ArgumentException("Invalid quest snapshot field");
            int fishType=numbers[^1];
            if(fishType<0||fishType>=65536||
                numbers.Take(kind==4?1:2).Any(value=>value!=0&&value!=1)||
                (kind==5&&parts.Length==4&&(numbers[2]<0||numbers[2]>255)))
                throw new ArgumentException("Quest snapshot outside supported bounds");
            Assembly assembly=BunnyRecipeLoader.LoadAssembly(modId,assemblyPath);
            string key=modId+"\n"+owner;
            if(!handlers.TryGetValue(key,out Handler? handler)||handler.Path!=assemblyPath)
            {
                Type[] types=assembly.GetTypes().Where(type=>type.Name==owner&&type.IsClass&&
                    !type.IsAbstract&&!type.ContainsGenericParameters&&typeof(BunnyItem).IsAssignableFrom(type)).ToArray();
                if(types.Length!=1)throw new InvalidOperationException("Quest owner identity mismatch");
                Type type=types[0];
                var declaration=type.GetCustomAttribute<BunnyContentAttribute>(false);
                if(declaration==null&&type.BaseType!=typeof(BunnyItem))
                    throw new InvalidOperationException("Inherited quest owner requires BunnyContent declaration");
                if(declaration!=null&&declaration.Kind!=BunnyContentKind.Item)
                    throw new InvalidOperationException("Quest owner declaration must be Item");
                var item=(BunnyItem)Activator.CreateInstance(type)!;
                if(declaration!=null)
                {
                    item.DisplayName=declaration.DisplayName;
                    item.Texture=declaration.Texture;
                }
                item.SetDefaults();
                if(declaration!=null&&(item.DisplayName!=declaration.DisplayName||
                    item.Texture!=declaration.Texture))
                    throw new InvalidOperationException("Quest owner changed declared content identity");
                if(!item.QuestFish)throw new InvalidOperationException("Quest owner must declare QuestFish");
                handler=new Handler{Path=assemblyPath,Item=item};
                handlers[key]=handler;
            }
            var context=new BunnyQuestContext{FishType=fishType,
                PlayerIndex=kind==5&&parts.Length==4?numbers[2]:-1,
                Hardmode=kind==4&&numbers[0]!=0,
                InvertedGravity=kind==5&&numbers[0]!=0,
                Uncommon=kind==5&&numbers[1]!=0};
            Func<BunnyQuestContext,bool>? predicate=kind==4?
                handler.Item.QuestAvailableWhen:handler.Item.QuestCanCatchWhen;
            if(predicate==null)throw new InvalidOperationException("Undeclared quest callback");
            return predicate(context)?"1":"0";
        }
        catch(Exception error)
        {
            return "ERROR|[CSharp][AnglerQuest] Mod="+modId+" Class="+owner+
                " Kind="+kind+" Stage=ManagedCallback Reason="+error.GetType().Name;
        }
    }
}

/// <summary>Compatibility endpoint retained for existing native bridges and source mods.</summary>
public static class BunnyRecipeRuntime
{
    internal static void Capture(BunnyRecipe recipe) => BunnyRecipeLoader.Capture(recipe);
    public static string Dispatch(string modId,string assemblyPath,string owner,int index,int kind,string snapshot)
        => BunnyRecipeLoader.Dispatch(modId,assemblyPath,owner,index,kind,snapshot);
}
