using System;
using System.Linq;
using BunnyLoader.Source;
using Original = Terraria.ModLoader;

static class UpstreamParity
{
    internal static int Run()
    {
        int checks = 0;
        void Same(bool equal, string label)
        {
            if (!equal) throw new Exception("UPSTREAM_PARITY_FAIL " + label);
            checks++;
        }
        var original = new OriginalLoader();
        original.Initialize(7);
        var bunny = LoaderManager.Get<ParityLoader>();
        var owner = new Original.Mod { Name = "parity" };
        var oldA = new OriginalDefinition { Mod = owner };
        var oldB = new OriginalDefinition { Mod = owner };
        int idA = original.Register(oldA);
        // Separate mod owners permit identical local names, matching Bunny scopes.
        oldB.Mod = new Original.Mod { Name = "parity-second" };
        int idB = original.Register(oldB);
        var a = new ParityDefinition();
        var b = new ParityDefinition();
        BunnyContentLoader.Load("parity", typeof(ModType).Assembly, mod => mod.AddContent(a));
        Original.ContentInstance.Register(oldA);
        Same(ReferenceEquals(Original.ContentInstance<OriginalDefinition>.Instance, oldA) &&
            ReferenceEquals(ContentInstance<ParityDefinition>.Instance, a), "single singleton");
        BunnyContentLoader.Load("parity-second", typeof(ModType).Assembly, mod => mod.AddContent(b));
        Same(a.Type == idA && b.Type == idB && bunny.TotalCount == original.TotalCount, "reserve/count");
        for (int id = -1; id < 12; id++)
            Same((bunny.Get(id) != null) == (original.Get(id) != null), "Get boundary " + id);
        Same(ReferenceEquals(original.Get(7), oldA) && ReferenceEquals(bunny.Get(7), a), "Get identity");
        Original.ModTypeLookup<OriginalDefinition>.RegisterLegacyNames(oldA, "before", "older");
        ModTypeLookup<ParityDefinition>.RegisterLegacyNames(a, "before", "older");
        foreach (string name in new[] { "ParityDefinition", "before", "older", "absent" })
        {
            // Upstream CLR class has a different name; its override supplies the same content name.
            bool upstreamFound = Original.ModTypeLookup<OriginalDefinition>.TryGetValue("parity", name, out _);
            bool bunnyFound = ModContent.TryFind<ParityDefinition>("parity", name, out _);
            Same(upstreamFound == bunnyFound, "tiered lookup " + name);
            Same(Original.ModTypeLookup<OriginalDefinition>.TryGetValue("parity/" + name, out _) ==
                ModContent.TryFind<ParityDefinition>("parity/" + name, out _), "flat lookup " + name);
        }
        // Drive upstream's real static-cache implementation, not a copy of Bunny logic.
        Original.ContentInstance.Register(oldB);
        Same((Original.ContentInstance<OriginalDefinition>.Instance == null) == (ContentInstance<ParityDefinition>.Instance == null), "multiple singleton");
        Same(Original.ContentInstance<OriginalDefinition>.Instances.Count == ContentInstance<ParityDefinition>.Instances!.Count, "multiple list");
        Same(Original.ContentInstance<OriginalDefinition>.Instances.Select(i => i.Mod.Name).SequenceEqual(
            ContentInstance<ParityDefinition>.Instances!.Select(i => i.Mod!.Name)), "multiple order");
        Terraria.ModLoader.Core.TypeCaching.Clear();
        BunnyContentLoader.UnloadAll();
        original.Unload();
        LoaderManager.Unload();
        Same(original.TotalCount == bunny.TotalCount && bunny.TotalCount == 7, "reset count");
        Same(Original.ContentInstance<OriginalDefinition>.Instance == null && ContentInstance<ParityDefinition>.Instance == null &&
            Original.ContentInstance<OriginalDefinition>.Instances == null && ContentInstance<ParityDefinition>.Instances == null, "clear cache");
        Same(!Original.ModTypeLookup<OriginalDefinition>.TryGetValue("parity/before", out _) &&
            !ModContent.TryFind<ParityDefinition>("parity/before", out _), "clear aliases");
        LoaderManager.Clear();
        return checks;
    }
    private sealed class OriginalDefinition : Original.ModType { public override string Name => "ParityDefinition"; }
    private sealed class OriginalLoader : Original.Loader<OriginalDefinition> { }
    [Autoload(false)]
    private sealed class ParityLoader : Loader<ParityDefinition> { public ParityLoader() => Initialize(7); }
    [Autoload(false)]
    private sealed class ParityDefinition : ModType
    {
        public int Type;
        protected override void Register() => Type = LoaderManager.Get<ParityLoader>().Register(this);
    }
}
