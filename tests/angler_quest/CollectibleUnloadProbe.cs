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
        var assembly=active.GetType().GetField("Assembly")?.GetValue(active) as Assembly;
        var context=assembly==null?null:AssemblyLoadContext.GetLoadContext(assembly);
        if(context==null||!context.IsCollectible)
            throw new InvalidOperationException("Collectible probe loaded a permanent assembly");
        var weak=new WeakReference(context);
        if(BunnyRecipeRuntime.Dispatch(modId,binding,"__unload__",0,6,"")!="CLEARED")
            throw new InvalidOperationException("Collectible probe unload failed");
        if(mods.Contains(modId))
            throw new InvalidOperationException("Collectible probe cache still owns mod");
        return weak;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference Rebind(string assemblyPath)
    {
        const string modId="collectible_rebind_probe";
        string original=assemblyPath+"|bunny-revision=1";
        string newer=assemblyPath+"|bunny-revision=2";
        if(BunnyRecipeRuntime.Dispatch(modId,original,"DynamicQuestFish",0,4,"1,950")!="1")
            throw new InvalidOperationException("Original mod callback failed");
        var mods=typeof(BunnyRecipeLoader).GetField("mods",
            BindingFlags.NonPublic|BindingFlags.Static)?.GetValue(null) as IDictionary;
        object previous=mods?[modId]??throw new InvalidOperationException("Missing original mod");
        var oldAssembly=previous.GetType().GetField("Assembly")?.GetValue(previous) as Assembly;
        var oldContext=oldAssembly==null?null:AssemblyLoadContext.GetLoadContext(oldAssembly);
        if(oldContext==null)throw new InvalidOperationException("Missing old context");
        var oldWeak=new WeakReference(oldContext);
        if(BunnyRecipeRuntime.Dispatch(modId,newer,"DynamicQuestFish",0,5,"1,1,2,950")!="1")
            throw new InvalidOperationException("Rebound callback failed");
        object latest=mods[modId]??throw new InvalidOperationException("Missing rebound mod");
        var newAssembly=latest.GetType().GetField("Assembly")?.GetValue(latest) as Assembly;
        var context=newAssembly==null?null:AssemblyLoadContext.GetLoadContext(newAssembly);
        if(context==null)throw new InvalidOperationException("Missing new context");
        if(ReferenceEquals(oldContext,context))
            throw new InvalidOperationException("Rebind reused existing assembly context");
        return oldWeak;
    }

    private static void WaitForRelease(WeakReference weak,string reason)
    {
        for(int attempt=0;attempt<12&&weak.IsAlive;++attempt)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
        if(weak.IsAlive)throw new InvalidOperationException(reason);
    }

    public static void AssertCollected(string assemblyPath)
    {
        WaitForRelease(Unload(assemblyPath),
            "Unload did not release collectible mod assembly");
        WaitForRelease(Rebind(assemblyPath),
            "Hot rebind did not release old collectible mod assembly");
        string cleanup=BunnyRecipeRuntime.Dispatch(
            "collectible_rebind_probe",assemblyPath+"|bunny-revision=2",
            "__unload__",0,6,"");
        if(cleanup!="CLEARED")
            throw new InvalidOperationException("Rebound mod cleanup failed");
    }
}
