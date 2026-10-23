using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using System.Text;
using BunnyLoader.SourceCompiler;

string work = Path.Combine(Path.GetTempPath(), "bunny-angler-host-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(work);
try
{
    string diagnostic = Path.Combine(work, "verify.log");
    string assembly = Assembly.GetExecutingAssembly().Location;
    int verified = CompiledAssemblyProbe.Verify(assembly, diagnostic);
    if (verified < 1)
        throw new InvalidOperationException("Fixture compilation or content verification failed: " +
            File.ReadAllText(diagnostic));
    string values = File.ReadAllText(Path.Combine(work, "source-runtime-values.txt"));
    if (!values.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        !values.Contains("\nITEM\tExampleQuestFish\t", StringComparison.Ordinal))
        throw new InvalidOperationException("Primary quest fish item missing from V2 output");
    string[] lines = values.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    string[] fish = lines.Single(line => line.StartsWith("QUEST_FISH\tExampleQuestFish\t", StringComparison.Ordinal)).Split('\t');
    if (fish.Length != 7 || fish[1] != "ExampleQuestFish" || fish[2] != "0" || fish[3] != "1" || fish[6] != "0")
        throw new InvalidOperationException("Quest availability or identity does not match the original fish");
    string description = Encoding.UTF8.GetString(Convert.FromHexString(fish[4]));
    string location = Encoding.UTF8.GetString(Convert.FromHexString(fish[5]));
    if (!description.Contains("fish that swims upside-down", StringComparison.Ordinal) ||
        location != "Caught anywhere while standing upside-down.")
        throw new InvalidOperationException("Original Bunny fish dialogue did not survive compiler wire");
    string[] locale = lines.Single(line => line.StartsWith("QUEST_LOCALE\t", StringComparison.Ordinal)).Split('\t');
    if (locale.Length != 5 || locale[1] != "ExampleQuestFish" || locale[2] != "pt-BR" ||
        !Encoding.UTF8.GetString(Convert.FromHexString(locale[3])).Contains("peixe que nada de ponta-cabeça", StringComparison.Ordinal) ||
        Encoding.UTF8.GetString(Convert.FromHexString(locale[4])) != "Pescado em qualquer lugar, de ponta-cabeça.")
        throw new InvalidOperationException("Original Portuguese quest dialogue missing from compiled wire");
    string[] catchRow = lines.Single(line => line.StartsWith("QUEST_CATCH\t", StringComparison.Ordinal)).Split('\t');
    if (catchRow.Length != 4 || catchRow[1] != "ExampleFishingPlayer" ||
        catchRow[2] != "ExampleQuestFish" || catchRow[3] != "3")
        throw new InvalidOperationException("Independent BunnyPlayer fishing policy missing from compiled wire");
    string[] dynamic = lines.Single(line => line.StartsWith("QUEST_CALLBACK\tDynamicQuestFish\t", StringComparison.Ordinal)).Split('\t');
    if (dynamic.Length != 3 || dynamic[2] != "3")
        throw new InvalidOperationException("Dynamic CSharp quest handlers were not serialized");
    string allowed = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly, "DynamicQuestFish", 0, 4, "1,950");
    string unavailable = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly, "DynamicQuestFish", 0, 4, "0,950");
    string caught = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly, "DynamicQuestFish", 0, 5, "1,1,2,950");
    string missed = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly, "DynamicQuestFish", 0, 5, "1,0,950");
    string otherPlayer = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly, "DynamicQuestFish", 0, 5, "1,1,1,950");
    string invalidOwner = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly, "DynamicQuestFish", 0, 5, "1,1,256,950");
    string invalid = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly, "DynamicQuestFish", 0, 5, "1,2,950");
    if (allowed != "1" || unavailable != "0" || caught != "1" ||
        missed != "0" || otherPlayer != "0" ||
        !invalidOwner.StartsWith("ERROR|", StringComparison.Ordinal) ||
        !invalid.StartsWith("ERROR|", StringComparison.Ordinal))
        throw new InvalidOperationException("Managed quest callbacks failed: " +
            string.Join(";", allowed, unavailable, caught, missed, otherPlayer, invalidOwner, invalid));
    string revisionOne = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly+"|bunny-revision=1",
        "DynamicQuestFish", 0, 4, "1,950");
    string revisionTwo = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly+"|bunny-revision=2",
        "DynamicQuestFish", 0, 5, "1,1,2,950");
    string malformedRevision = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly+"|bunny-revision=bad",
        "DynamicQuestFish", 0, 4, "1,950");
    if(revisionOne!="1"||revisionTwo!="1"||
       !malformedRevision.StartsWith("ERROR|",StringComparison.Ordinal))
        throw new InvalidOperationException("Managed assembly revision rebinding failed: "+
            string.Join(";",revisionOne,revisionTwo,malformedRevision));
    string[] attributed = lines.Single(line =>
        line.StartsWith("QUEST_CALLBACK\tAttributedQuestFish\t", StringComparison.Ordinal)).Split('\t');
    if (attributed.Length != 3 || attributed[2] != "2")
        throw new InvalidOperationException("Inherited BunnyContent quest callback mask incorrect");
    string declaredFish = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly+"|bunny-revision=3",
        "AttributedQuestFish", 0, 5, "1,1,2,954");
    string declaredWrongOwner = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly+"|bunny-revision=3",
        "AttributedQuestFish", 0, 5, "1,1,1,954");
    if (declaredFish != "1" || declaredWrongOwner != "0")
        throw new InvalidOperationException("Inherited quest declaration was not honored by Mono: "+
            string.Join(";",declaredFish,declaredWrongOwner));
    string otherMod = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "other_angler_mod", assembly+"|bunny-revision=1",
        "DynamicQuestFish", 0, 4, "1,950");
    string newestMod = BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture", assembly+"|bunny-revision=4",
        "AttributedQuestFish", 0, 5, "1,1,2,954");
    if(otherMod!="1"||newestMod!="1")
        throw new InvalidOperationException("Quest assembly revision or separate mod failed");
    var loadedMods=typeof(BunnyLoader.Source.BunnyRecipeLoader).GetField(
        "mods",BindingFlags.Static|BindingFlags.NonPublic)?.GetValue(null)
        as System.Collections.IDictionary;
    if(loadedMods==null || loadedMods.Count!=2)
        throw new InvalidOperationException("Collectible CSharp mod contexts were not registered");
    foreach(string modId in new[]{"angler_host_fixture","other_angler_mod"})
    {
        var activeMod=loadedMods[modId]??throw new InvalidOperationException("Missing cached mod");
        var context=activeMod.GetType().GetField("Context")?.GetValue(activeMod)
            as AssemblyLoadContext;
        var modAssembly=activeMod.GetType().GetField("Assembly")?.GetValue(activeMod)
            as Assembly;
        if(context==null || modAssembly==null || !context.IsCollectible ||
            !ReferenceEquals(context,AssemblyLoadContext.GetLoadContext(modAssembly)))
            throw new InvalidOperationException("CSharp mod assembly is not in a collectible per-mod context");
        Type owner=modAssembly.GetTypes().Single(type=>type.Name=="DynamicQuestFish");
        if(!typeof(BunnyLoader.Source.BunnyItem).IsAssignableFrom(owner))
            throw new InvalidOperationException("Collectible mod duplicated BunnySourceSdk type identity");
    }
    var handles=typeof(BunnyLoader.Source.BunnyQuestRuntime).GetField(
        "handlers", BindingFlags.Static|BindingFlags.NonPublic)?.GetValue(null)
        as System.Collections.IDictionary;
    if(handles==null||handles.Count!=2||
        !handles.Contains("angler_host_fixture\nAttributedQuestFish")||
        !handles.Contains("other_angler_mod\nDynamicQuestFish")||
        handles.Contains("angler_host_fixture\nDynamicQuestFish"))
        throw new InvalidOperationException("Stale quest delegates retained after managed assembly rebind");
    string malformedUnload=BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture",assembly+"|bunny-revision=4","__unload__",1,6,"");
    if(!malformedUnload.StartsWith("ERROR|",StringComparison.Ordinal)||handles.Count!=2)
        throw new InvalidOperationException("Managed unload accepted malformed request");
    string cleared=BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture",assembly+"|bunny-revision=4","__unload__",0,6,"");
    string staleUnload=BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "angler_host_fixture",assembly+"|bunny-revision=4","__unload__",0,6,"");
    if(cleared!="CLEARED"||!staleUnload.StartsWith("PENDING|",StringComparison.Ordinal)||
       handles.Count!=1||!handles.Contains("other_angler_mod\nDynamicQuestFish"))
        throw new InvalidOperationException("Managed unload retained quest delegates or erased another mod");
    string otherCleared=BunnyLoader.Source.BunnyRecipeRuntime.Dispatch(
        "other_angler_mod",assembly+"|bunny-revision=1","__unload__",0,6,"");
    if(otherCleared!="CLEARED"||handles.Count!=0)
        throw new InvalidOperationException("Second mod cache not released on unload");
    UniversalHookUnloadProbe.Run(assembly);
    CollectibleUnloadProbe.AssertCollected(assembly);
    Console.WriteLine("ANGLER_QUEST_COMPILED_ASSEMBLY_OK");
}
finally
{
    Directory.Delete(work, recursive: true);
}
