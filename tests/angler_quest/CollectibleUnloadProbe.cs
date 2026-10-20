using System;
using System.Collections;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using BunnyLoader.Source;

internal static class CollectibleUnloadProbe
{
    [MethodImpl(MethodImplOptions.NoInlining)]
    public static WeakReference Unload(string assemblyPath)
    {
        const string modId="collectible_unload_probe";
        string binding=assemblyPath+"|bunny-revision=1";
        string allowed=BunnyRecipeRuntime.Dispatch(modId,binding,"DynamicQuestFish",0,4,"1,950");
        if(allowed!="1")throw new InvalidOperationException("Collectible probe callback failed");
        var mods=typeof(BunnyRecipeLoader).GetField("mods",
            BindingFlags.NonPublic|BindingFlags.Static)?.GetValue(null) as IDictionary;
        object active=mods?[modId]??throw new InvalidOperationException("Collectible probe missing");
        var context=active.GetType().GetField("Context")?.GetValue(active) as AssemblyLoadContext;
        if(context==null||!context.IsCollectible)
            throw new InvalidOperationException("Collectible probe loaded a permanent assembly");
        var weak=new WeakReference(context);
        if(BunnyRecipeRuntime.Dispatch(modId,binding,"__unload__",0,6,"")!="CLEARED")
            throw new InvalidOperationException("Collectible probe unload failed");
        if(mods.Contains(modId))
            throw new InvalidOperationException("Collectible probe cache still owns mod");
        return weak;
    }

    public static void AssertCollected(string assemblyPath)
    {
        var weak=Unload(assemblyPath);
        for(int attempt=0;attempt<12&&weak.IsAlive;++attempt)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
        if(weak.IsAlive)
            throw new InvalidOperationException("Unload did not release collectible mod assembly");
    }
}
