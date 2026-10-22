using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Threading.Tasks;
using BunnyLoader.Source;
using ContentLoaderExample;

int checks = 0;
void Check(bool ok, string name)
{
    if (!ok) throw new Exception("FAIL " + name);
    checks++;
}
TError Throws<TError>(Action action, string name) where TError : Exception
{
    try { action(); }
    catch (TError error) { Check(true, name); return error; }
    throw new Exception("FAIL " + name + " did not throw " + typeof(TError).Name);
}
Assembly empty = typeof(ModType).Assembly;
BunnyContentMod Manual(string uid, params ModType[] content) => BunnyContentLoader.Load(uid, empty, mod => {
    foreach (ModType instance in content) mod.AddContent(instance);
});

// Execute the real example assembly, including deterministic discovery, Load, Register,
// resize, cross-content SetupContent, name lookup, legacy name and full cleanup.
var sample = ExampleBootstrap.Load();
Check(sample.IsLoaded && sample.GetContent<ExampleDefinition>().Count == 2, "real sample autoload");
Check(ExampleBootstrap.ReadConfiguredLife() == 240, "real sample dependency in SetupContent");
var difficulty = ModContent.Find<ExampleDefinition>(ExampleBootstrap.ModUid, "ExampleDifficultySettings");
Check(ReferenceEquals(difficulty, ModContent.Find<ExampleDefinition>(ExampleBootstrap.ModUid + "/OldDifficultySettings")), "attribute alias");
Check(difficulty.Type == 4 && LoaderManager.Get<ExampleDefinitionLoader>().Get(4) == difficulty, "reserve and indexed retrieval");
Check(LoaderManager.Get<ExampleDefinitionLoader>().Get(3) == null && LoaderManager.Get<ExampleDefinitionLoader>().Get(6) == null, "vanilla/out of range");
Check(ReferenceEquals(LoaderManager.Get<ExampleDefinitionLoader>(), LoaderManager.Get<ExampleDefinitionLoader>()), "cached loader");
Throws<InvalidOperationException>(() => LoaderManager.Get<ExampleDefinitionLoader>().Initialize(6), "cannot reinitialize loaded IDs");
Throws<AggregateException>(() => LoaderManager.Unload(), "cannot reset a populated loader");
Throws<InvalidOperationException>(() => BunnyContentLoader.Load(sample.Name, sample.Assembly), "duplicate mod");
Check(BunnyContentLoader.IsLoaded(sample.Name), "duplicate mod rejection preserves existing scope");
Check(!ExampleBootstrap.TryReadOptionalSettings("absent", out var missing) && missing == null, "optional dependency absent");
Check(!ModContent.TryFind<ExampleDefinition>("EXAMPLE-content-catalog/ExampleDifficultySettings", out _), "case sensitive");
Check(ModContent.GetInstance<ExampleDefinition>() == null && ContentInstance<ExampleDefinition>.Instances == null, "singleton exact CLR type");
var snapshot = ModContent.GetContent<ExampleDefinition>();
Throws<NotSupportedException>(() => ((IList<ExampleDefinition>)snapshot).Clear(), "snapshot immutable");
ExampleBootstrap.Unload();
Check(!BunnyContentLoader.IsLoaded(sample.Name) && ModContent.GetInstance<ExampleDifficultySettings>() == null, "unload scope and singleton");
Check(!ModContent.TryFind<ExampleDefinition>(sample.Name + "/OldDifficultySettings", out _) && LoaderManager.Get<ExampleDefinitionLoader>().Get(4) == null, "aliases and indexed entries removed");
Check(snapshot.Count == 2, "caller-held snapshot is a snapshot, not a live collection");
Throws<InvalidOperationException>(() => sample.AddContent(new Basic("late")), "closed load phase");
LoaderManager.Clear();
Check(ExampleBootstrap.Load().GetContent<ExampleDefinition>().Count == 2 && difficulty.Mod == null, "reload after complete teardown");
ExampleBootstrap.Unload();
LoaderManager.Clear();

// Registry tables are per T, as in upstream; same names in different categories are legal.
var a = new Basic("same");
var b = new Other("same");
var first = Manual("one", a, b);
Check(ModContent.Find<Basic>("one/same") == a && ModTypeLookup<Other>.Get("one", "same") == b, "separate category tables");
Check(!ModContent.TryFind<ModType>("one/same", out _), "no invented base category registration");
Check(ModContent.GetContent<ModType>().Count == 2, "GetContent includes derived types without aliases");
Check(ModContent.GetInstance<Basic>() == a && ContentInstance<Basic>.Instances == null, "single instance upstream semantics");
var a2 = new Basic("second");
Manual("two", a2);
Check(ContentInstance<Basic>.Instance == null && ContentInstance<Basic>.Instances!.Count == 2, "multi instance upstream semantics");
Check(ReferenceEquals(ContentInstance<Basic>.Instances, ContentInstance<Basic>.Instances), "cached immutable multi-instance snapshot");
Throws<NotSupportedException>(() => ((IList<Basic>)ContentInstance<Basic>.Instances!).Clear(), "instances immutable");
BunnyContentLoader.Unload("two");
Check(ContentInstance<Basic>.Instance == a && ContentInstance<Basic>.Instances == null, "multiple to single transition");
ModTypeLookup<Basic>.RegisterLegacyNames(a, "previous", "older");
Check(ModTypeLookup<Basic>.TryGetValue("one", "previous", out var alias) && alias == a, "manual aliases");
Throws<InvalidOperationException>(() => ModTypeLookup<Basic>.RegisterLegacyNames(a, "not-published", "same"), "alias batch duplicate rejection");
Check(!ModContent.TryFind<Basic>("one/not-published", out _), "alias batch atomic");
Throws<InvalidOperationException>(() => ModTypeLookup<Basic>.RegisterLegacyNames(a, "twice", "twice"), "duplicate in alias batch");
Throws<InvalidOperationException>(() => ModTypeLookup<Basic>.Register(a), "register twice");
Check(first.GetContent<Basic>().Count == 1, "aliases excluded from GetContent");
Check(!BunnyContentLoader.Unload("missing"), "unknown mod unload");

// Fault injection: rollback must remove names, aliases and singleton references for the
// failed mod while preserving every previously loaded mod. Include the currently failing Load.
var loadFault = new Basic("load-fault") { FailLoad = true };
Throws<InvalidOperationException>(() => Manual("bad-load", loadFault), "Load error propagated");
Check(loadFault.Unloads == 1 && loadFault.Mod == null && !BunnyContentLoader.IsLoaded("bad-load"), "current Load cleanup");
Check(ContentInstance<Basic>.Instance == a, "failed mod did not corrupt previous singleton");
var setupFault = new Basic("setup-fault") { FailSetup = true };
Throws<InvalidOperationException>(() => Manual("bad-setup", setupFault), "SetupContent error");
Check(!ModContent.TryFind<Basic>("bad-setup/setup-fault", out _) && setupFault.Unloads == 1, "SetupContent rollback");
var duplicateA = new Basic("dupe");
var duplicateB = new Basic("dupe");
Throws<InvalidOperationException>(() => Manual("bad-name", duplicateA, duplicateB), "duplicate content name");
Check(duplicateA.Mod == null && duplicateB.Mod == null && !ModContent.TryFind<Basic>("bad-name/dupe", out _), "duplicate rollback");
var aggregateFault = new Basic("aggregate") { FailLoad = true, FailUnload = true };
Check(Throws<AggregateException>(() => Manual("aggregate", aggregateFault), "rollback errors aggregate").InnerExceptions.Count == 2, "both errors preserved");
Check(aggregateFault.Mod == null, "cleanup despite faulty rollback");
var disabled = new Basic("disabled") { Enabled = false };
Check(Manual("disabled", disabled).GetContent<Basic>().Count == 0 && disabled.Mod == null && disabled.Loads == 0, "IsLoadingEnabled respected");
BunnyContentLoader.Unload("disabled");

// AddContent from a Load callback is allowed, recursive scope mutations are not.
var child = new Basic("child");
var parent = new Basic("parent") { OnLoad = mod => mod.AddContent(child) };
Manual("nested-content", parent);
Check(ModContent.Find<Basic>("nested-content/child") == child && child.Setups == 1, "AddContent during Load");
BunnyContentLoader.Unload("nested-content");
var recursive = new Basic("recursive") { OnLoad = _ => BunnyContentLoader.UnloadAll() };
Throws<InvalidOperationException>(() => Manual("recursive", recursive), "recursive unload rejected");
Check(ModContent.Find<Basic>("one/same") == a, "recursive mutation cannot clear another mod");
var late = new Basic("late-setup") { OnSetup = mod => mod.AddContent(new Basic("too-late")) };
Throws<InvalidOperationException>(() => Manual("late", late), "AddContent forbidden during SetupContent");

// Constructor failure during true reflection autoload; nothing gets published.
var dyn = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName("FaultyContent"), AssemblyBuilderAccess.Run);
var builder = dyn.DefineDynamicModule("main").DefineType("BadCtor", TypeAttributes.Public, typeof(ModType));
var ctor = builder.DefineConstructor(MethodAttributes.Public, CallingConventions.Standard, Type.EmptyTypes).GetILGenerator();
ctor.Emit(OpCodes.Ldarg_0);
ctor.Emit(OpCodes.Call, typeof(ModType).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null, Type.EmptyTypes, null)!);
ctor.Emit(OpCodes.Newobj, typeof(InvalidOperationException).GetConstructor(Type.EmptyTypes)!);
ctor.Emit(OpCodes.Throw);
var register = builder.DefineMethod("Register", MethodAttributes.Family | MethodAttributes.Virtual, typeof(void), Type.EmptyTypes);
register.GetILGenerator().Emit(OpCodes.Ret); // Not invoked: constructor fails first.
builder.CreateType();
Throws<TargetInvocationException>(() => BunnyContentLoader.Load("bad-ctor", dyn), "autoload constructor failure");
Check(!BunnyContentLoader.IsLoaded("bad-ctor") && ContentInstance<Basic>.Instance == a, "constructor error isolation");

// A swallowed callback error still aborts the transaction: failed content is never published as loaded.
Throws<InvalidOperationException>(() => BunnyContentLoader.Load("caught-error", empty, mod => {
    try { mod.AddContent(new Basic("caught") { FailLoad = true }); } catch (InvalidOperationException) { }
}), "caught AddContent error aborts commit");
Check(!BunnyContentLoader.IsLoaded("caught-error") && !ModContent.TryFind<Basic>("caught-error/caught", out _), "caught failure rollback");

// Full teardown visits all mods despite user exceptions; subsequent load remains usable.
Manual("faulty-unload", new Basic("unload") { FailUnload = true });
Manual("after-faulty", new Basic("last"));
Throws<AggregateException>(() => BunnyContentLoader.UnloadAll(), "aggregate unload");
Check(ModContent.GetContent<ModType>().Count == 0 && ContentInstance<Basic>.Instance == null && ContentInstance<Basic>.Instances == null, "all references cleared");
Check(!ModContent.TryFind<Basic>("one/previous", out _) && a.Mod == null, "all names and aliases cleared");
LoaderManager.Clear();

// Autoload follows inherited flags, constructor availability, generic/abstract exclusion and side.
BunnyContentLoader.ExecutionSide = ModSide.Client;
var clientScope = BunnyContentLoader.Load("client", typeof(Program).Assembly);
Check(clientScope.GetContent<AutoContent>().Count == 2 && ModContent.GetInstance<ClientOnly>() != null &&
    ModContent.GetInstance<ServerOnly>() == null && ModContent.GetInstance<NoSyncOnly>() != null, "client side autoload");
Check(ModContent.GetInstance<InheritedOff>() == null && ModContent.GetInstance<NoDefaultConstructor>() == null, "inherited disabled/nondefault constructor");
Throws<InvalidOperationException>(() => BunnyContentLoader.ExecutionSide = ModSide.Server, "cannot change active side");
BunnyContentLoader.UnloadAll();
LoaderManager.Clear();
BunnyContentLoader.ExecutionSide = ModSide.Server;
var serverScope = BunnyContentLoader.Load("server", typeof(Program).Assembly);
Check(serverScope.GetContent<AutoContent>().Count == 2 && ModContent.GetInstance<ServerOnly>() != null &&
    ModContent.GetInstance<ClientOnly>() == null, "server side autoload");
BunnyContentLoader.UnloadAll();
LoaderManager.Clear();
BunnyContentLoader.ExecutionSide = ModSide.Client;
Throws<ArgumentOutOfRangeException>(() => BunnyContentLoader.ExecutionSide = ModSide.NoSync, "invalid execution context");

// Loader lifecycle failures aggregate without skipping later loaders or corrupting enumeration.
var faultLoader = LoaderManager.Get<FaultLoader>();
var visitedLoader = LoaderManager.Get<VisitedLoader>();
Throws<AggregateException>(() => LoaderManager.ResizeArrays(), "loader callbacks aggregate");
Check(visitedLoader.Visits == 1, "later loader still resized");
Check(ReferenceEquals(faultLoader, LoaderManager.Get<FaultLoader>()), "recursive Clear cannot drop manager references");
LoaderManager.Clear();

// True loader discovery failure: construct A, fail Z, cleanup A, publish neither.
var loaderDyn = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName("FaultyLoaders"), AssemblyBuilderAccess.Run);
var loaderModule = loaderDyn.DefineDynamicModule("main");
var goodBuilder = loaderModule.DefineType("AConstructed", TypeAttributes.Public, typeof(Loader));
var goodCtor = goodBuilder.DefineConstructor(MethodAttributes.Public, CallingConventions.Standard, Type.EmptyTypes).GetILGenerator();
goodCtor.Emit(OpCodes.Ldarg_0);
goodCtor.Emit(OpCodes.Call, typeof(Loader).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null, Type.EmptyTypes, null)!);
goodCtor.Emit(OpCodes.Call, typeof(ConstructorTracker).GetMethod(nameof(ConstructorTracker.RecordConstruction))!);
goodCtor.Emit(OpCodes.Ret);
var cleanupMethod = goodBuilder.DefineMethod("Unload", MethodAttributes.Public | MethodAttributes.Virtual, typeof(void), Type.EmptyTypes);
cleanupMethod.GetILGenerator().Emit(OpCodes.Call, typeof(ConstructorTracker).GetMethod(nameof(ConstructorTracker.RecordCleanup))!);
cleanupMethod.GetILGenerator().Emit(OpCodes.Ret);
var goodType = goodBuilder.CreateType()!;
var badBuilder = loaderModule.DefineType("ZBadConstructor", TypeAttributes.Public, typeof(Loader));
var badCtor = badBuilder.DefineConstructor(MethodAttributes.Public, CallingConventions.Standard, Type.EmptyTypes).GetILGenerator();
badCtor.Emit(OpCodes.Ldarg_0);
badCtor.Emit(OpCodes.Call, typeof(Loader).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null, Type.EmptyTypes, null)!);
badCtor.Emit(OpCodes.Newobj, typeof(InvalidOperationException).GetConstructor(Type.EmptyTypes)!);
badCtor.Emit(OpCodes.Throw);
badBuilder.CreateType();
Throws<TargetInvocationException>(() => LoaderManager.AutoLoad(loaderDyn), "loader discovery constructor failure");
Check(ConstructorTracker.Constructions == 1 && ConstructorTracker.Cleanups == 1, "pending loader cleaned");
typeof(LoaderManager).GetMethod(nameof(LoaderManager.Get))!.MakeGenericMethod(goodType).Invoke(null, null);
Check(ConstructorTracker.Constructions == 2, "failed discovery did not cache partial result");
LoaderManager.Clear();

var equalsFirst = new EqualityTrap("first");
var equalsSecond = new EqualityTrap("second");
Manual("equals-a", equalsFirst);
Manual("equals-b", equalsSecond);
BunnyContentLoader.Unload("equals-a");
Check(ReferenceEquals(ContentInstance<EqualityTrap>.Instance, equalsSecond), "cleanup uses reference identity, not user Equals");
BunnyContentLoader.UnloadAll();
var directLoader = LoaderManager.Get<DirectLoader>();
LoaderManager.ResizeArrays();
Check(ReferenceEquals(directLoader, LoaderManager.Get<DirectLoader>()), "default ILoader methods supported");
LoaderManager.Clear();

// Repeated load/unload and parallel read snapshots exercise catalog synchronization.
for (int i = 0; i < 40; i++)
{
    var instance = new Basic("entry");
    Manual("repeat", instance);
    Parallel.For(0, 100, _ => {
        if (ModContent.Find<Basic>("repeat/entry") != instance || ModContent.GetInstance<Basic>() != instance)
            throw new Exception("parallel lookup mismatch");
    });
    Check(BunnyContentLoader.Unload("repeat") && instance.Mod == null && ContentInstance<Basic>.Instance == null, "repeat #" + i);
}

Throws<ArgumentException>(() => BunnyContentLoader.Load("../bad", empty), "invalid mod name");
Throws<ArgumentException>(() => ModContent.Find<Basic>("unqualified"), "unqualified name");
Throws<ArgumentException>(() => ModContent.Find<Basic>("mod/a/b"), "invalid full name");
Throws<ArgumentNullException>(() => ModContent.Find<Basic>(null!), "null key");
Throws<ArgumentException>(() => Manual("bad-local", new Basic("bad/name")), "invalid local name");
Throws<KeyNotFoundException>(() => ModContent.Find<Basic>("missing/content"), "Find missing");
Throws<InvalidOperationException>(() => ModTypeLookup<Basic>.Register(new Basic("unowned")), "cannot publish unowned instance");
Check(ModContent.GetContent<ModType>().Count == 0, "final catalog empty");
checks += UpstreamParity.Run();
Console.WriteLine("BUNNY_CONTENT_LOADER_PASS checks=" + checks);

[Autoload(false)]
sealed class Basic : ModType
{
    private readonly string name;
    public Basic(string name) => this.name = name;
    public override string Name => name;
    public bool Enabled = true, FailLoad, FailSetup, FailUnload;
    public int Loads, Setups, Unloads;
    public Action<BunnyContentMod>? OnLoad, OnSetup;
    public override bool IsLoadingEnabled(BunnyContentMod mod) => Enabled;
    public override void Load() { Loads++; OnLoad?.Invoke(Mod!); if (FailLoad) throw new InvalidOperationException("load failed"); }
    protected override void Register() => ModTypeLookup<Basic>.Register(this);
    public override void SetupContent() => SetStaticDefaults();
    public override void SetStaticDefaults() { Setups++; OnSetup?.Invoke(Mod!); if (FailSetup) throw new InvalidOperationException("setup failed"); }
    public override void Unload() { Unloads++; if (FailUnload) throw new InvalidOperationException("unload failed"); }
}
[Autoload(false)]
sealed class Other : ModType
{
    private readonly string name;
    public Other(string name) => this.name = name;
    public override string Name => name;
    protected override void Register() => ModTypeLookup<Other>.Register(this);
}

[Autoload(false)]
abstract class AutoContent : ModType
{
    protected override void Register() => ModTypeLookup<AutoContent>.Register(this);
}
[Autoload(Side = ModSide.Client)]
sealed class ClientOnly : AutoContent { }
[Autoload(Side = ModSide.Server)]
sealed class ServerOnly : AutoContent { }
[Autoload(Side = ModSide.NoSync)]
sealed class NoSyncOnly : AutoContent { }
sealed class InheritedOff : AutoContent { }
[Autoload]
sealed class NoDefaultConstructor : AutoContent { public NoDefaultConstructor(int value) { } }
[Autoload]
sealed class OpenGeneric<T> : AutoContent { }
[Autoload(false)]
sealed class FaultLoader : Loader
{
    public override void ResizeArrays() => LoaderManager.Clear();
}
[Autoload(false)]
sealed class VisitedLoader : Loader
{
    public int Visits;
    public override void ResizeArrays() => Visits++;
}

public static class ConstructorTracker
{
    public static int Constructions, Cleanups;
    public static void RecordConstruction() => Constructions++;
    public static void RecordCleanup() => Cleanups++;
}
[Autoload(false)]
sealed class EqualityTrap : ModType
{
    private readonly string name;
    public EqualityTrap(string name) => this.name = name;
    public override string Name => name;
    protected override void Register() => ModTypeLookup<EqualityTrap>.Register(this);
    public override bool Equals(object? obj) => obj is EqualityTrap;
    public override int GetHashCode() => 0;
}
[Autoload(false)]
sealed class DirectLoader : ILoader { }
