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
    private readonly List<string> grants = new();
    internal string Grants => string.Join(";",grants);
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
        if(count<1||count>9999||grants.Count>=8)throw new ArgumentOutOfRangeException(nameof(count));
        grants.Add(key+":"+count.ToString(CultureInfo.InvariantCulture));
    }
}

/// <summary>Internal transport endpoint, public for Mono reflection. All callbacks execute on the existing Mono worker.</summary>
public static class BunnyRecipeRuntime
{
    private sealed class Mod
    {
        public string Path="";
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
                mod=new Mod{Path=assemblyPath};
                foreach(Type type in assembly.GetTypes().Where(t=>t.IsClass&&!t.IsAbstract&&!t.ContainsGenericParameters&&typeof(BunnyItem).IsAssignableFrom(t)))
                {
                    var item=(BunnyItem)Activator.CreateInstance(type)!;
                    item.SetDefaults();capture=new List<BunnyRecipe>();
                    try { item.AddRecipes();mod.Recipes.Add(type.Name,capture); }
                    finally { capture=null; }
                }
                mods[modId]=mod;
            }
            if(!mod.Recipes.TryGetValue(owner,out var recipes)||(uint)index>=(uint)recipes.Count)
                throw new InvalidOperationException("Recipe handler identity not registered");
            if(kind==3)return "READY"; // Bind and reconstruct handlers before the native recipe is acknowledged.
            string[] p=snapshot.Split(',');
            if(p.Length!=9)throw new ArgumentException("Expected nine snapshot fields");
            int[] n=p.Select(v=>int.Parse(v,CultureInfo.InvariantCulture)).ToArray();
            if(n[0]<0||n[1]<1||n[2]<0||n[2]>31||n[5]<0||n[5]>9999)throw new ArgumentException("Snapshot outside native contract");
            var context=new BunnyCraftContext{CurrentLife=n[0],MaxLife=n[1],IsDay=(n[2]&1)!=0,
                InDesert=(n[2]&2)!=0,NearWater=(n[2]&4)!=0,InSnow=(n[2]&8)!=0,
                QuickCraft=(n[2]&16)!=0,ResultType=n[3],AvailableIngredientCount=n[6]};
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
