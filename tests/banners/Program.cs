using BunnyLoader.Source;
using ExampleModCSharp.Examples.Banners;

int passed = 0;

void Check(string name, Action action)
{
    action();
    Console.WriteLine("PASS " + name);
    passed++;
}
void True(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}
void Throws<T>(Action action) where T : Exception
{
    try { action(); }
    catch (T) { return; }
    throw new Exception("Excecao esperada: " + typeof(T).Name);
}

Check("dois NPCs dividem a mesma bandeira", () =>
{
    var registry = new BunnyBannerRegistry(700);
    ExampleBannerRegistration.Register(registry);
    True(ExampleBannerRegistration.Bind(registry, 700, 701, 2100), "vinculo");
    True(registry.Count == 2 && registry.RequiredSlots == 701, "contagem");
    True(registry.NpcToBanner(700) == 700, "principal");
    True(registry.NpcToBanner(701) == 700, "variante");
    True(registry.BannerToItem(700) == 2100, "item");
    True(registry.ItemToBanner(2100) == 700, "reverso");
    True(registry.NpcToBanner(10) == 0 && registry.BannerToItem(2) == 0, "vanilla");
});

Check("registro exige nomes estaveis e nao duplica", () =>
{
    var registry = new BunnyBannerRegistry(700);
    registry.Register(new BunnyBannerDefinition("mod_a", "Slime", "Slime", "Banner"));
    Throws<ArgumentException>(() => registry.Register(
        new BunnyBannerDefinition("mod_a", "Slime", "Slime", "Banner")));
    Throws<ArgumentException>(() => registry.Register(
        new BunnyBannerDefinition("mod/a", "Bad", "Bad", "Banner")));
    Throws<ArgumentException>(() => registry.Register(
        new BunnyBannerDefinition("mod_a", "Bad Name", "Bad", "Banner")));
    Throws<ArgumentOutOfRangeException>(() => new BunnyBannerRegistry(0));
    True(registry.Count == 0, "houve vinculo parcial");
});

Check("tipos vanilla e indices fora do limite sao recusados", () =>
{
    var registry = new BunnyBannerRegistry(700);
    registry.Register(new BunnyBannerDefinition("m", "N", "N", "I"));
    True(!registry.TryBind("m", "N", 699, 700, 1000), "npc vanilla");
    True(!registry.TryBind("m", "N", 700, 699, 1000), "banner vanilla");
    True(!registry.TryBind("m", "N", 700, 700, 0), "item zero");
    True(!registry.TryBind("m", "N", BunnyBannerRegistry.MaximumBannerSlots, 700, 1000), "max");
    True(registry.Count == 0, "bind parcial");
    True(registry.TryBind("m", "N", 700, 700, 1000), "bind valido");
});

Check("conflitos nao sobrescrevem mappings existentes", () =>
{
    var registry = new BunnyBannerRegistry(700);
    registry.Register(new BunnyBannerDefinition("m", "A", "A", "One"));
    registry.Register(new BunnyBannerDefinition("m", "B", "A", "Other"));
    registry.Register(new BunnyBannerDefinition("m", "C", "C", "One"));
    True(registry.TryBind("m", "A", 700, 700, 1000), "A");
    True(!registry.TryBind("m", "B", 701, 700, 1001), "banner conflitante");
    True(!registry.TryBind("m", "C", 702, 702, 1000), "item conflitante");
    True(!registry.TryBind("m", "C", 700, 702, 1002), "npc duplicado");
    True(registry.Count == 1 && registry.BannerToItem(700) == 1000, "rollback");
    True(registry.NpcToBanner(701) == 0 && registry.ItemToBanner(1001) == 0, "residuos");
});

Check("resize cresce tudo sem mutar arrays de entrada", () =>
{
    var initial = new BunnyBannerArrays(
        new[] { 8, 9 }, new[] { 1 }, new[] { true },
        new[] { true, false, true }, new[] { false });
    var grown = BunnyBannerRegistry.Grow(initial, 8);
    True(grown.KillCount.Length == 8 && grown.ClaimableBanners.Length == 8
         && grown.NewBanner.Length == 8 && grown.SceneBuff.Length == 8
         && grown.PlayerSceneBuff.Length == 8, "tamanhos");
    True(grown.KillCount[1] == 9 && grown.ClaimableBanners[0] == 1 &&
         grown.SceneBuff[2], "dados preservados");
    True(initial.KillCount.Length == 2 && initial.SceneBuff.Length == 3, "arrays originais");
    Throws<ArgumentOutOfRangeException>(() => BunnyBannerRegistry.Grow(initial, 262145));
});

Check("captura e restaura progresso no novo mundo com IDs novos", () =>
{
    var a = new BunnyBannerRegistry(700);
    ExampleBannerRegistration.Register(a);
    True(ExampleBannerRegistration.Bind(a, 700, 701, 2000), "bind A");
    var kills = new int[704];
    var claim = new int[704];
    var fresh = new bool[704];
    kills[2] = 88;
    kills[700] = 149;
    claim[700] = 2;
    fresh[700] = true;
    var sidecar = a.Capture(kills, claim, fresh);
    True(sidecar.Count == 2, "duas identidades por npc representante");
    var b = new BunnyBannerRegistry(700);
    ExampleBannerRegistration.Register(b);
    True(ExampleBannerRegistration.Bind(b, 707, 708, 2100), "bind B");
    var nk = new int[710];
    var nc = new int[710];
    var nf = new bool[710];
    nk[2] = 99;
    b.Load(sidecar);
    b.Restore(nk, nc, nf);
    True(nk[707] == 149 && nc[707] == 2 && nf[707], "restaurado");
    True(nk[2] == 99 && nk[700] == 0, "nao tocou vanilla ou tipo antigo");
});

Check("dados de mod ausente sobrevivem a troca de mod", () =>
{
    var registry = new BunnyBannerRegistry(700);
    registry.Load(new Dictionary<string, BunnyBannerState>
    {
        ["no_longer_installed/RareSlime"] = new(45, 1, true),
        ["invalid/key/extra"] = new(22, 0, false)
    });
    var snap = registry.Capture(new int[700], new int[700], new bool[700]);
    True(snap.TryGetValue("no_longer_installed/RareSlime", out var value) &&
         value.KillCount == 45, "estado preservado");
    True(!snap.ContainsKey("invalid/key/extra"), "chave invalida nao entrou");
    registry.ResetWorld();
    True(registry.Capture(new int[700], new int[700], new bool[700]).Count == 0,
         "reset de mundo");
});

Check("snapshot corrompido nao injeta negativos ou Fresh isolado", () =>
{
    var registry = new BunnyBannerRegistry(700);
    registry.Register(new BunnyBannerDefinition("m", "N", "N", "Item"));
    True(registry.TryBind("m", "N", 700, 700, 2000), "bind");
    registry.Load(new Dictionary<string, BunnyBannerState> {
        ["m/N"] = new(-20, int.MaxValue, true)
    });
    var kills = new int[701];
    var claim = new int[701];
    var fresh = new bool[701];
    registry.Restore(kills, claim, fresh);
    True(kills[700] == 0 && claim[700] == ushort.MaxValue && fresh[700],
         "clamp");
    registry.Load(new Dictionary<string, BunnyBannerState> { ["m/N"] = new(20, 0, true) });
    registry.Restore(kills, claim, fresh);
    True(claim[700] == 0 && !fresh[700], "fresh sem claim");
});

Check("arrays pequenos sao recusados sem escrita parcial", () =>
{
    var registry = new BunnyBannerRegistry(700);
    registry.Register(new BunnyBannerDefinition("m", "A", "A", "I"));
    True(registry.TryBind("m", "A", 700, 700, 2000), "bind");
    registry.Load(new Dictionary<string, BunnyBannerState> { ["m/A"] = new(50, 1, true) });
    var kills = new int[701];
    kills[700] = 111;
    Throws<ArgumentException>(() => registry.Restore(kills, new int[700], new bool[701]));
    True(kills[700] == 111, "escrita parcial");
    Throws<ArgumentException>(() => registry.Capture(kills, new int[700], new bool[701]));
});

Check("unload preserva sidecar mas libera tipos e items", () =>
{
    var registry = new BunnyBannerRegistry(700);
    registry.Register(new BunnyBannerDefinition("m", "A", "A", "I"));
    True(registry.TryBind("m", "A", 700, 700, 2000), "bind");
    var kill = new int[701];
    kill[700] = 21;
    registry.Capture(kill, new int[701], new bool[701]);
    registry.Unregister("m");
    True(registry.Count == 0 && registry.ItemToBanner(2000) == 0, "indices");
    True(registry.Capture(kill, new int[701], new bool[701]).ContainsKey("m/A"),
         "sidecar");
});

Console.WriteLine("BannerLoader C# source contracts: " + passed + " PASS");
