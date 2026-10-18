using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
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
public static class BunnyRecipeRuntime
{
    private sealed class Mod
    {
        public string Path="";
        public Assembly Assembly=null!;
        public Dictionary<string,List<BunnyRecipe>> Recipes=new(StringComparer.Ordinal);
    }
    private static readonly Dictionary<string,Mod> mods=new(StringComparer.Ordinal);
    [ThreadStatic] private static List<BunnyRecipe>? capture;
    internal static void Capture(BunnyRecipe recipe) { capture?.Add(recipe); }
    public static string Dispatch(string modId,string assemblyPath,string owner,int index,int kind,string snapshot)
    {
        try
        {
            if(!mods.TryGetValue(modId,out Mod? mod)||mod.Path!=assemblyPath)
            {
                Assembly assembly=Assembly.Load(File.ReadAllBytes(assemblyPath));
                mod=new Mod{Path=assemblyPath,Assembly=assembly};
                mods[modId]=mod;
            }
            if(!mod.Recipes.TryGetValue(owner,out var recipes))
            {
                    Type type=mod.Assembly.GetTypes().Single(t=>t.Name==owner&&t.IsClass&&!t.IsAbstract&&!t.ContainsGenericParameters&&typeof(BunnyItem).IsAssignableFrom(t));
                    var item=(BunnyItem)Activator.CreateInstance(type)!;
                    var declaration=type.GetCustomAttribute<BunnyContentAttribute>(false);
                    if(declaration!=null){
                        if(declaration.Kind!=BunnyContentKind.Item)throw new InvalidOperationException("Declared recipe owner must be BunnyContentKind.Item: "+type.FullName);
                        item.DisplayName=declaration.DisplayName;item.Texture=declaration.Texture;
                    }
                    item.SetDefaults();
                    if(declaration!=null&&(item.DisplayName!=declaration.DisplayName||item.Texture!=declaration.Texture))
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
