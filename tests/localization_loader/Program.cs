using BunnyLoader.Source;
using System.Collections.Generic;
using System.Threading.Tasks;

int passed = 0;
void Equal<T>(T expected, T actual)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
        throw new Exception($"Expected '{expected}', got '{actual}'");
}
void Reject(Action action)
{
    try { action(); }
    catch (ArgumentException) { return; }
    catch (InvalidOperationException) { return; }
    throw new Exception("Invalid localization data was accepted");
}
void Test(string name, Action action)
{
    action();
    passed++;
    Console.WriteLine("PASS " + name);
}

Test("fallback-english-then-first", () =>
{
    var catalog = new BunnyLocalizationCatalog("pt-BR");
    catalog.RegisterCulture("examplemod", "es-ES", new Dictionary<string, string> { ["Items.Foo.DisplayName"] = "Español" });
    Equal("Español", catalog.Translate("Items.Foo.DisplayName", "examplemod"));
    catalog.RegisterCulture("examplemod", "en-US", new Dictionary<string, string> { ["Items.Foo.DisplayName"] = "English" });
    Equal("English", catalog.Translate("Items.Foo.DisplayName", "examplemod"));
    catalog.RegisterCulture("examplemod", "pt-BR", new Dictionary<string, string> { ["Items.Foo.DisplayName"] = "Português" });
    Equal("Português", catalog.Translate("Items.Foo.DisplayName", "examplemod"));
    catalog.SetCulture("fr-FR");
    Equal("English", catalog.Translate("Items.Foo.DisplayName", "examplemod"));
    Equal("fr-FR", catalog.ActiveCultureName);
});

Test("scope-and-cross-mod", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("one", "en-US", new Dictionary<string, string> { ["Items.Label"] = "one" });
    Equal("one", catalog.Translate("Items.Label"));
    catalog.RegisterCulture("two", "en-US", new Dictionary<string, string> { ["Items.Label"] = "two" });
    Equal("", catalog.TryTranslate("Items.Label"));
    Equal("Items.Label", catalog.Translate("Items.Label"));
    Equal("two", catalog.Translate("Items.Label", "two"));
    Equal("two", catalog.Translate("Mods.two.Items.Label", "one"));
    Equal(false, catalog.Exists("Missing", "one"));
    Equal("Missing", catalog.Translate("Missing", "one"));
});

Test("references-local-absolute-offset", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("one", "en-US", new Dictionary<string, string>
    {
        ["Common.Greeting"] = "Hello {0} {^1:variant}",
        ["Items.Greeting.Tooltip"] = "{$Common.Greeting@2}!",
        ["Items.Greeting.DisplayName"] = "{$Mods.one.Common.Greeting}",
        ["Items.Empty"] = ""
    });
    Equal("Hello {2} {^3:variant}!", catalog.Translate("Items.Greeting.Tooltip", "one"));
    Equal("Hello {0} {^1:variant}", catalog.Translate("Items.Greeting.DisplayName", "one"));
    Equal("", catalog.Translate("Items.Empty", "one"));
    Equal("", catalog.TryTranslate("NotFound"));
});

Test("references-cycle-and-unknown", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("cycle", "en-US", new Dictionary<string, string>
    {
        ["A"] = "{$B}", ["B"] = "{$A}", ["C"] = "x {$Missing}"
    });
    Equal("Mods.cycle.A", catalog.Translate("A", "cycle"));
    Equal("x Missing", catalog.Translate("C", "cycle"));
});

Test("snapshot-published-and-removal", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("one", "en-US", new Dictionary<string, string> { ["Greeting"] = "old" });
    var copy = catalog.SnapshotForGame();
    int revision = catalog.Revision;
    catalog.RegisterCulture("one", "en-US", new Dictionary<string, string> { ["Greeting"] = "new" });
    Equal("old", copy["Mods.one.Greeting"]);
    Equal("new", catalog.Translate("Greeting", "one"));
    Equal(true, catalog.Revision > revision);
    catalog.RemoveMod("one");
    Equal(0, catalog.Count);
    Equal("", catalog.TryTranslate("Greeting", "one"));
    Equal("old", copy["Mods.one.Greeting"]);
});

Test("global-overrides-and-language-switch", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.Register("LegacyInterface.28", "en-US", "Menu");
    catalog.Register("LegacyInterface.28", "pt-BR", "Menu PT");
    catalog.Register("LegacyInterface.28", "en-US", "Replaced");
    Equal("Replaced", catalog.Translate("LegacyInterface.28"));
    catalog.SetCulture("pt-BR");
    Equal("Menu PT", catalog.Translate("LegacyInterface.28"));
    Equal("Menu PT", catalog.SnapshotForGame()["LegacyInterface.28"]);
});

Test("bounded-validation-atomic", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    Reject(() => catalog.RegisterCulture("../bad", "en-US", new Dictionary<string, string>()));
    Reject(() => catalog.RegisterCulture("good", "en_US", new Dictionary<string, string>()));
    Reject(() => catalog.RegisterCulture("good", "en-US", new Dictionary<string, string> { ["Bad..Key"] = "value" }));
    Reject(() => catalog.RegisterCulture("good", "en-US", new Dictionary<string, string>
        { ["Good"] = "not-published", ["Invalid"] = new string('x', 8193) }));
    Equal(0, catalog.Count);
    Reject(() => catalog.Register("bad", "en-US", "value"));
    Reject(() => catalog.Register("Category.Key", "en-US", "bad\0text"));
    Equal(0, catalog.Count);
});

Test("concurrent-culture-updates", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("one", "en-US", new Dictionary<string, string> { ["Greeting"] = "en" });
    catalog.RegisterCulture("one", "pt-BR", new Dictionary<string, string> { ["Greeting"] = "pt" });
    Parallel.For(0, 12000, index =>
    {
        if (index % 19 == 0) catalog.SetCulture(index % 2 == 0 ? "pt-BR" : "en-US");
        string answer = catalog.Translate("Greeting", "one");
        if (answer != "en" && answer != "pt") throw new Exception("Torn localization read");
    });
});

Test("original-examplemod-json-both-cultures", () =>
{
    string content = Path.Combine(Directory.GetCurrentDirectory(),
        "samples", "ExampleModCSharp", "Playable", "content");
    var catalog = new BunnyLocalizationCatalog("en-US");
    Equal(2, catalog.LoadFromContent("examplemodcsharp", content));
    Equal(308, catalog.Count);
    Equal("Example Item", catalog.Translate("ItemName.ExampleItem", "examplemodcsharp"));
    catalog.SetCulture("pt-BR");
    Equal("Exemplo de Item", catalog.Translate("ItemName.ExampleItem", "examplemodcsharp"));
    catalog.SetCulture("fr-FR");
    Equal("Example Item", catalog.Translate("ItemName.ExampleItem", "examplemodcsharp"));
});

Test("atomic-mod-replace-removes-stale-keys", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("example", "en-US", new Dictionary<string, string> { ["Stale"] = "old" });
    catalog.ReplaceModCultures("example",
        new Dictionary<string, IReadOnlyDictionary<string, string>>
        {
            ["en-US"] = new Dictionary<string, string> { ["Current"] = "now" },
            ["pt-BR"] = new Dictionary<string, string> { ["Current"] = "agora" }
        });
    Equal(false, catalog.Exists("Stale", "example"));
    Equal("now", catalog.Translate("Current", "example"));
    var immutable = catalog.SnapshotForGame();
    Reject(() => catalog.ReplaceModCultures("example",
        new Dictionary<string, IReadOnlyDictionary<string, string>>
        {
            ["en-US"] = new Dictionary<string, string> { ["Change"] = "invalid" },
            ["pt-BR"] = new Dictionary<string, string> { ["Bad..Key"] = "bad" }
        }));
    Equal("now", catalog.Translate("Current", "example"));
    Equal(true, immutable.ContainsKey("Mods.example.Current"));
});

Test("malformed-json-does-not-poison-existing-mod", () =>
{
    string temp = Path.Combine(Path.GetTempPath(), "bunny-localization-" + Guid.NewGuid().ToString("N"));
    string localization = Path.Combine(temp, "Localization");
    Directory.CreateDirectory(localization);
    try
    {
        var catalog = new BunnyLocalizationCatalog();
        catalog.RegisterCulture("sample", "en-US", new Dictionary<string, string> { ["Name"] = "intact" });
        File.WriteAllText(Path.Combine(localization, "en-US.json"),
            "{ \"ItemName\": { \"ExampleItem\": \"Olá \\uD83D\\uDE03\" }, \"Ignore\": [1, 2, null] }");
        Equal(1, catalog.LoadFromContent("sample", temp));
        Equal("Olá 😃", catalog.Translate("ItemName.ExampleItem", "sample"));
        File.WriteAllText(Path.Combine(localization, "pt-BR.json"), "{ \"ItemName\": {\"Key\": \"Unclosed\" ");
        bool rejected = false;
        try { catalog.LoadFromContent("sample", temp); }
        catch (InvalidDataException) { rejected = true; }
        Equal(true, rejected);
        Equal("Olá 😃", catalog.Translate("ItemName.ExampleItem", "sample"));
        File.Delete(Path.Combine(localization, "pt-BR.json"));
        File.WriteAllText(Path.Combine(localization, "en-US.json"), "{ \"Bad\": \"\\uDE03\" }");
        rejected = false;
        try { catalog.LoadFromContent("sample", temp); }
        catch (InvalidDataException) { rejected = true; }
        Equal(true, rejected);
        Equal("Olá 😃", catalog.Translate("ItemName.ExampleItem", "sample"));
    }
    finally { Directory.Delete(temp, true); }
});

Test("locale-wire-culture-utf8-deterministic", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("source", "pt-BR", new Dictionary<string, string> { ["ItemName.ExampleItem"] = "Exemplo de Item" });
    catalog.RegisterCulture("source", "en-US", new Dictionary<string, string> { ["ItemName.ExampleItem"] = "Example Item" });
    var wire = catalog.ExportWireRows();
    Equal(2, wire.Count);
    Equal("LOCALE", wire[0].Split('\t')[0]);
    Equal("en-US", wire[0].Split('\t')[1]);
    Equal("pt-BR", wire[1].Split('\t')[1]);
    Equal("Mods.source.ItemName.ExampleItem", System.Text.Encoding.UTF8.GetString(
        Convert.FromHexString(wire[0].Split('\t')[2])));
    Equal("Exemplo de Item", System.Text.Encoding.UTF8.GetString(
        Convert.FromHexString(wire[1].Split('\t')[3])));
    Equal(true, wire.All(value => value.Split('\t').Length == 4));
});

Test("language-manager-variants", () =>
{
    var catalog = new BunnyLocalizationCatalog();
    catalog.RegisterCulture("variantmod", "en-US", new Dictionary<string, string>
    {
        ["ItemName.ExampleItem"] = "Default",
        ["ItemName.ExampleItem$Alt"] = "Alternative"
    });
    Equal("Alternative", catalog.Translate("ItemName.ExampleItem$Alt", "variantmod"));
    var wire = catalog.ExportWireRows();
    Equal(2, wire.Count);
    Equal(true, wire.Any(row => System.Text.Encoding.UTF8.GetString(
        Convert.FromHexString(row.Split('\t')[2])).EndsWith("ExampleItem$Alt", StringComparison.Ordinal)));
    Reject(() => catalog.RegisterCulture("variantmod", "en-US",
        new Dictionary<string, string> { ["ItemName.A$"] = "bad" }));
    Reject(() => catalog.RegisterCulture("variantmod", "en-US",
        new Dictionary<string, string> { ["ItemName.A$B.Other"] = "bad" }));
    Reject(() => catalog.RegisterCulture("variantmod", "en-US",
        new Dictionary<string, string> { ["ItemName.A$B$C"] = "bad" }));
    Equal(2, catalog.Count);
});

Test("strict-json-numbers-and-transactional-reload", () =>
{
    string root = Path.Combine(Path.GetTempPath(), "bunny-locale-numeric-" + Guid.NewGuid().ToString("N"));
    string folder = Path.Combine(root, "Localization");
    Directory.CreateDirectory(folder);
    try
    {
        var catalog = new BunnyLocalizationCatalog();
        string file = Path.Combine(folder, "en-US.json");
        File.WriteAllText(file, "{\"Label\":\"Original\",\"Ignored\":[0,-7,2.5,1e3,-0.02E-4]}");
        Equal(1, catalog.LoadFromContent("numeric", root));
        Equal("Original", catalog.Translate("Label", "numeric"));
        string[] malformed = { "01", "-01", "1.", "1e", "1e+", "--1", "2x", "+1", ".3", "1.2.3" };
        foreach (string bad in malformed)
        {
            File.WriteAllText(file, "{\"Label\":\"New\",\"Ignored\":" + bad + "}");
            bool rejected = false;
            try { catalog.LoadFromContent("numeric", root); }
            catch (InvalidDataException) { rejected = true; }
            Equal(true, rejected);
            Equal("Original", catalog.Translate("Label", "numeric"));
        }
    }
    finally { Directory.Delete(root, true); }
});

Test("register-language-map-atomic-and-key-identity", () =>
{
    var catalog = new BunnyLocalizationCatalog("pt-BR");
    catalog.Register("LegacyInterface.28", new Dictionary<string, string>
    {
        ["en-US"] = "Open",
        ["pt-BR"] = "Abrir"
    });
    Equal("Abrir", catalog.Translate("LegacyInterface.28"));
    catalog.SetCulture("en-US");
    Equal("Open", catalog.Translate("LegacyInterface.28"));
    catalog.Register("Mods.owner.ItemName.Foo", new Dictionary<string, string>
    {
        ["en-US"] = "Foo",
        ["pt-BR"] = "Fó"
    });
    Equal("Mods.owner.ItemName.Foo", catalog.Key("ItemName.Foo", "owner"));
    Equal("Mods.owner.ItemName.Foo", catalog.Key("Mods.owner.ItemName.Foo", "owner"));
    Equal("ItemName.Missing", catalog.Key("ItemName.Missing", "owner"));
    Equal("Mods.other.ItemName.Foo", catalog.Key("Mods.other.ItemName.Foo", "owner"));
    int revision = catalog.Revision;
    Reject(() => catalog.Register("LegacyInterface.28", new Dictionary<string, string>
    {
        ["pt-BR"] = "Novo",
        ["de_DE"] = "Ungültig"
    }));
    Equal(revision, catalog.Revision);
    catalog.SetCulture("pt-BR");
    Equal("Abrir", catalog.Translate("LegacyInterface.28"));
    catalog.Register("LegacyInterface.28", new Dictionary<string, string>
    {
        ["en-US"] = "Updated"
    });
    Equal("Abrir", catalog.Translate("LegacyInterface.28"));
    catalog.SetCulture("fr-FR");
    Equal("Updated", catalog.Translate("LegacyInterface.28"));
    Reject(() => catalog.Register("LegacyInterface.28",
        new Dictionary<string, string>()));
    Equal("Updated", catalog.Translate("LegacyInterface.28"));
});

Test("texts-by-culture-expanded-without-mutation", () =>
{
    var catalog = new BunnyLocalizationCatalog("pt-BR");
    catalog.RegisterCulture("owner", "en-US", new Dictionary<string, string>
    {
        ["Common.Welcome"] = "Hello {0}",
        ["Items.Foo.Tooltip"] = "{$Common.Welcome@1}"
    });
    catalog.RegisterCulture("owner", "pt-BR", new Dictionary<string, string>
    {
        ["Common.Welcome"] = "Olá {0}",
        ["Items.Foo.Tooltip"] = "{$Common.Welcome@1}"
    });
    var translations = catalog.Texts("Items.Foo.Tooltip", "owner");
    Equal(true, translations != null);
    Equal("Hello {1}", translations!["en-US"]);
    Equal("Olá {1}", translations["pt-BR"]);
    Equal(2, translations.Count);
    Equal(true, catalog.Texts("Items.Unknown", "owner") == null);
    Equal("Olá {1}", catalog.Translate("Items.Foo.Tooltip", "owner"));
    translations = catalog.Texts("Mods.owner.Items.Foo.Tooltip", "owner");
    Equal("Hello {1}", translations!["en-US"]);
    Equal("pt-BR", catalog.ActiveCultureName);
});

Test("register-precedence-survives-json-reload-and-unload", () =>
{
    var catalog = new BunnyLocalizationCatalog("en-US");
    catalog.RegisterCulture("owner", "en-US", new Dictionary<string, string>
    {
        ["Items.Label"] = "File original"
    });
    var held = catalog.SnapshotForGame();
    catalog.Register("Mods.owner.Items.Label", "en-US", "Explicit");
    catalog.RegisterCulture("owner", "en-US", new Dictionary<string, string>
    {
        ["Items.Label"] = "File changed"
    });
    Equal("Explicit", catalog.Translate("Items.Label", "owner"));
    catalog.ReplaceModCultures("owner",
        new Dictionary<string, IReadOnlyDictionary<string, string>>
        {
            ["en-US"] = new Dictionary<string, string> { ["Items.Label"] = "JSON reloaded" },
            ["pt-BR"] = new Dictionary<string, string> { ["Items.Label"] = "Arquivo" }
        });
    Equal("Explicit", catalog.Translate("Items.Label", "owner"));
    catalog.SetCulture("pt-BR");
    Equal("Arquivo", catalog.Translate("Items.Label", "owner"));
    catalog.Register("Mods.owner.Items.Label", new Dictionary<string, string>
    {
        ["pt-BR"] = "Registrado"
    });
    Equal("Registrado", catalog.Translate("Items.Label", "owner"));
    catalog.ReplaceModCultures("owner",
        new Dictionary<string, IReadOnlyDictionary<string, string>>
        {
            ["en-US"] = new Dictionary<string, string> { ["Items.Label"] = "File again" },
            ["pt-BR"] = new Dictionary<string, string> { ["Items.Label"] = "Arquivo de novo" }
        });
    Equal("Registrado", catalog.Translate("Items.Label", "owner"));
    catalog.SetCulture("en-US");
    Equal("Explicit", catalog.Translate("Items.Label", "owner"));
    Equal("File original", held["Mods.owner.Items.Label"]);
    catalog.RemoveMod("owner");
    Equal(false, catalog.Exists("Items.Label", "owner"));
    catalog.ReplaceModCultures("owner",
        new Dictionary<string, IReadOnlyDictionary<string, string>>
        {
            ["en-US"] = new Dictionary<string, string> { ["Items.Label"] = "Clean after unload" }
        });
    Equal("Clean after unload", catalog.Translate("Items.Label", "owner"));
});

Test("compiled-localization-module-publishes-wire-without-json", () =>
{
    string root = Path.Combine(Path.GetTempPath(), "bunny-localization-module-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(Path.Combine(root, "content"));
    try
    {
        string diagnostics = Path.Combine(root, "module-diagnostics.txt");
        int count = BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(
            System.Reflection.Assembly.GetExecutingAssembly().Location, diagnostics, root);
        Equal(1, count);
        string[] wire = File.ReadAllLines(Path.Combine(root, "source-runtime-values.txt"));
        Equal("BUNNY_MANAGED_DEFAULTS_V2", wire[0]);
        var locales = wire.Where(row => row.StartsWith("LOCALE\t", StringComparison.Ordinal)).ToArray();
        Equal(2, locales.Length);
        string first = System.Text.Encoding.UTF8.GetString(Convert.FromHexString(locales[0].Split('\t')[2]));
        Equal("Mods.source.ItemName.HostModule", first);
        Equal("en-US", locales[0].Split('\t')[1]);
        Equal("Host module", System.Text.Encoding.UTF8.GetString(Convert.FromHexString(locales[0].Split('\t')[3])));
        Equal("pt-BR", locales[1].Split('\t')[1]);
        Equal("Módulo de exemplo", System.Text.Encoding.UTF8.GetString(Convert.FromHexString(locales[1].Split('\t')[3])));
        Equal(true, File.ReadAllText(diagnostics).Contains("modules=1 rows=2", StringComparison.Ordinal));
    }
    finally { Directory.Delete(root, true); }
});

Test("compiled-localization-module-overrides-json-cultures", () =>
{
    string root = Path.Combine(Path.GetTempPath(), "bunny-localization-source-" + Guid.NewGuid().ToString("N"));
    string folder = Path.Combine(root, "content", "Localization");
    Directory.CreateDirectory(folder);
    try
    {
        File.WriteAllText(Path.Combine(folder, "en-US.json"),
            "{\"ItemName\":{\"HostModule\":\"JSON fallback\"}}");
        File.WriteAllText(Path.Combine(folder, "pt-BR.json"),
            "{\"ItemName\":{\"HostModule\":\"Texto do JSON\"}}");
        string diagnostics = Path.Combine(root, "module-diagnostics.txt");
        Equal(1, BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(
            System.Reflection.Assembly.GetExecutingAssembly().Location, diagnostics, root));
        var locales = File.ReadAllLines(Path.Combine(root, "source-runtime-values.txt"))
            .Where(row => row.StartsWith("LOCALE\t", StringComparison.Ordinal)).ToArray();
        Equal(2, locales.Length);
        Equal("Host module", System.Text.Encoding.UTF8.GetString(
            Convert.FromHexString(locales[0].Split('\t')[3])));
        Equal("Módulo de exemplo", System.Text.Encoding.UTF8.GetString(
            Convert.FromHexString(locales[1].Split('\t')[3])));
        Equal(true, File.ReadAllText(diagnostics).Contains("cultures=2 modules=1 rows=2", StringComparison.Ordinal));
    }
    finally { Directory.Delete(root, true); }
});

Test("localization-module-without-static-json-emits-marker", () =>
{
    string root = Path.Combine(Path.GetTempPath(), "bunny-locale-only-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(Path.Combine(root, "content"));
    string? prior = Environment.GetEnvironmentVariable("BUNNY_TEST_LOCALE_ONLY");
    Environment.SetEnvironmentVariable("BUNNY_TEST_LOCALE_ONLY", "1");
    try
    {
        string diagnostics = Path.Combine(root, "diagnostics.txt");
        Equal(1, BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(
            System.Reflection.Assembly.GetExecutingAssembly().Location, diagnostics, root));
        string[] wire = File.ReadAllLines(Path.Combine(root, "source-runtime-values.txt"));
        Equal(true, wire.Contains("LOCALE_MODULES\t1"));
        Equal(true, wire.Contains("LOCALE_RUNTIME\t1"));
        Equal(false, wire.Any(row => row.StartsWith("LOCALE\t", StringComparison.Ordinal)));
    }
    finally
    {
        Environment.SetEnvironmentVariable("BUNNY_TEST_LOCALE_ONLY", prior);
        Directory.Delete(root, true);
    }
});

Test("managed-localization-callback-lifecycle", () =>
{
    string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
    HostLocalizationModule.CallbackCount = 0;
    HostLocalizationModule.LastCulture = "";
    Equal("OK|1\nBUNNY_DYNAMIC_LOCALES_V1", BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US"));
    Equal(1, HostLocalizationModule.CallbackCount);
    Equal("en-US", HostLocalizationModule.LastCulture);
    Equal("en-US", BunnyLocalizationRuntime.ActiveCultureName);
    Equal("OK|1\nBUNNY_DYNAMIC_LOCALES_V1", BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "pt-BR"));
    Equal(2, HostLocalizationModule.CallbackCount);
    Equal("pt-BR", HostLocalizationModule.LastCulture);
    Equal("pt-BR", BunnyLocalizationRuntime.ActiveCultureName);
    Equal("ERROR|InvalidArguments", BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "pt_BR"));
    Equal(2, HostLocalizationModule.CallbackCount);
    Equal("ERROR|InvalidArguments", BunnyLocalizationRuntime.Dispatch("../other", assembly, "en-US"));
    Equal("ERROR|InvalidArguments", BunnyLocalizationRuntime.Dispatch("examplemod", "../bad.dll", "en-US"));
    HostLocalizationModule.ShouldFail = true;
    Equal("ERROR|CallbackFaults=1",
        BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US"));
    Equal("pt-BR", BunnyLocalizationRuntime.ActiveCultureName);
    HostLocalizationModule.ShouldFail = false;
    Equal(true, BunnyLocalizationRuntime.Unload("examplemod"));
    Equal(false, BunnyLocalizationRuntime.Unload("examplemod"));
    Equal("OK|1\nBUNNY_DYNAMIC_LOCALES_V1", BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US"));
    Equal(4, HostLocalizationModule.CallbackCount);
    Equal(true, BunnyLocalizationRuntime.Unload("examplemod"));
});

Test("managed-dynamic-translations-atomic-and-owned", () =>
{
    string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
    Reject(() => BunnyLocalizationRuntime.Register("Mods.examplemod.Common.Greeting", "en-US", "Outside callback"));
    HostLocalizationModule.DynamicTranslations = true;
    string wire = BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "pt-BR");
    var rows = wire.Split('\n');
    Equal("OK|1", rows[0]);
    Equal("BUNNY_DYNAMIC_LOCALES_V1", rows[1]);
    Equal(4, rows.Length);
    Equal("en-US", rows[2].Split('\t')[1]);
    Equal("pt-BR", rows[3].Split('\t')[1]);
    Equal("Mods.examplemod.Common.RuntimeGreeting", System.Text.Encoding.UTF8.GetString(
        Convert.FromHexString(rows[3].Split('\t')[2])));
    Equal("Olá em tempo de execução", System.Text.Encoding.UTF8.GetString(
        Convert.FromHexString(rows[3].Split('\t')[3])));
    HostLocalizationModule.RegisterForeign = true;
    Equal("ERROR|CallbackFaults=1", BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US"));
    HostLocalizationModule.RegisterForeign = false;
    HostLocalizationModule.DynamicTranslations = false;
    Equal("OK|1\nBUNNY_DYNAMIC_LOCALES_V1",
        BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US"));
    Equal(true, BunnyLocalizationRuntime.Unload("examplemod"));
});

Test("managed-callback-unload-race-is-serialized", () =>
{
    string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
    HostLocalizationModule.ShouldFail = false;
    HostLocalizationModule.DynamicTranslations = true;
    HostLocalizationModule.RegisterForeign = false;
    Parallel.For(0, 256, index =>
    {
        string culture = index % 2 == 0 ? "en-US" : "pt-BR";
        string result = BunnyLocalizationRuntime.Dispatch("examplemod", assembly, culture);
        if (!result.StartsWith("OK|1\nBUNNY_DYNAMIC_LOCALES_V1\n", StringComparison.Ordinal))
            throw new InvalidOperationException("Localization callback escaped the serialized unload lifecycle");
        if (index % 3 == 0) BunnyLocalizationRuntime.Unload("examplemod");
    });
    HostLocalizationModule.DynamicTranslations = false;
    Equal(true, BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US")
        .StartsWith("OK|1\nBUNNY_DYNAMIC_LOCALES_V1", StringComparison.Ordinal));
    BunnyLocalizationRuntime.Unload("examplemod");
    Equal("", BunnyLocalizationRuntime.ActiveCultureName);
});

Test("authored-vanilla-register-is-owned-by-caller", () =>
{
    string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
    HostLocalizationModule.GlobalRegistration = true;
    try
    {
        string wire = BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "pt-BR");
        string[] rows = wire.Split('\n');
        Equal("OK|1", rows[0]);
        Equal("BUNNY_DYNAMIC_LOCALES_V1", rows[1]);
        Equal(true, rows.Skip(2).Any(row => System.Text.Encoding.UTF8.GetString(
            Convert.FromHexString(row.Split('\t')[2])) == "LegacyInterface.28"));
        HostLocalizationModule.RegisterForeign = true;
        Equal("ERROR|CallbackFaults=1",
            BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US"));
    }
    finally
    {
        HostLocalizationModule.GlobalRegistration = false;
        HostLocalizationModule.RegisterForeign = false;
        BunnyLocalizationRuntime.Unload("examplemod");
    }
});

Test("compiled-vanilla-register-uses-source-wire", () =>
{
    string root = Path.Combine(Path.GetTempPath(), "bunny-locale-vanilla-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(Path.Combine(root, "content"));
    string? prior = Environment.GetEnvironmentVariable("BUNNY_TEST_GLOBAL_LOCALE");
    Environment.SetEnvironmentVariable("BUNNY_TEST_GLOBAL_LOCALE", "1");
    try
    {
        string diagnostics = Path.Combine(root, "diagnostics.txt");
        Equal(1, BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(
            System.Reflection.Assembly.GetExecutingAssembly().Location, diagnostics, root));
        string[] wire = File.ReadAllLines(Path.Combine(root, "source-runtime-values.txt"));
        Equal(true, wire.Any(row => row.StartsWith("LOCALE\ten-US\t", StringComparison.Ordinal) &&
            System.Text.Encoding.UTF8.GetString(Convert.FromHexString(row.Split('\t')[2])) == "LegacyInterface.28"));
    }
    finally
    {
        Environment.SetEnvironmentVariable("BUNNY_TEST_GLOBAL_LOCALE", prior);
        Directory.Delete(root, true);
    }
});

Test("live-register-outside-callback-drains-with-assembly-ownership", () =>
{
    string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
    string initial = BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US");
    Equal("OK|1\nBUNNY_DYNAMIC_LOCALES_V1", initial);
    Equal(initial, BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    Equal(false, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", "OK|0\nBUNNY_DYNAMIC_LOCALES_V1"));
    Equal(initial, BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    Equal(true, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", initial));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    BunnyLocalizationRuntime.Register("LegacyInterface.28",
        new Dictionary<string, string>
        {
            ["en-US"] = "Dynamic vanilla",
            ["pt-BR"] = "Vanilla em tempo real"
        });
    BunnyLocalizationRuntime.Register("Mods.examplemod.Common.Live", "pt-BR", "Registrado depois");
    string wire = BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US");
    string[] rows = wire.Split('\n');
    Equal("OK|1", rows[0]);
    Equal(5, rows.Length);
    Equal(true, rows.Skip(2).Any(row => System.Text.Encoding.UTF8.GetString(
        Convert.FromHexString(row.Split('\t')[2])) == "LegacyInterface.28"));
    Equal(wire, BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    Reject(() => BunnyLocalizationRuntime.Register("Mods.other.Common.Forbidden", "en-US", "No"));
    Equal(wire, BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    Equal(true, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", wire));
    Equal(false, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", wire));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    BunnyLocalizationRuntime.Register("Mods.examplemod.Common.Live", "pt-BR", "Atualizado");
    string updated = BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US");
    Equal(true, updated.Contains(Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("Atualizado")), StringComparison.Ordinal));
    Equal(false, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", wire));
    Equal(updated, BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    string brazil = BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "pt-BR");
    Equal("OK|1", brazil.Split('\n')[0]);
    Equal(false, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", updated));
    Equal(brazil, BunnyLocalizationRuntime.Drain("examplemod", assembly, "pt-BR"));
    Equal(true, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "pt-BR", brazil));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("examplemod", assembly, "pt-BR"));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    Equal(true, BunnyLocalizationRuntime.Unload("examplemod"));
    Reject(() => BunnyLocalizationRuntime.Register("LegacyInterface.28", "en-US", "After unload"));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
});

Test("assembly-identity-ambiguity-denies-live-registration", () =>
{
    string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
    string one = BunnyLocalizationRuntime.Dispatch("one", assembly, "en-US");
    string two = BunnyLocalizationRuntime.Dispatch("two", assembly, "en-US");
    Equal("OK|1", one.Split('\n')[0]);
    Equal("OK|1", two.Split('\n')[0]);
    Reject(() => BunnyLocalizationRuntime.Register("LegacyInterface.28", "en-US", "Ambiguous"));
    Equal("", BunnyLocalizationRuntime.ActiveCultureName);
    Equal(true, BunnyLocalizationRuntime.Acknowledge("one", assembly, "en-US", one));
    Equal(true, BunnyLocalizationRuntime.Acknowledge("two", assembly, "en-US", two));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("one", assembly, "en-US"));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("two", assembly, "en-US"));
    Equal(true, BunnyLocalizationRuntime.Unload("one"));
    BunnyLocalizationRuntime.Register("Mods.two.Common.Safe", "en-US", "Safe");
    string pending = BunnyLocalizationRuntime.Drain("two", assembly, "en-US");
    Equal(true, pending.Contains(Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes("Mods.two.Common.Safe")), StringComparison.Ordinal));
    Equal(true, BunnyLocalizationRuntime.Acknowledge("two", assembly, "en-US", pending));
    Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("two", assembly, "en-US"));
    Equal(true, BunnyLocalizationRuntime.Unload("two"));
});

Test("combined-dynamic-localization-limit-is-atomic", () =>
{
    string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
    HostLocalizationModule.SeedCallbackLimit = true;
    try
    {
        string callback = BunnyLocalizationRuntime.Dispatch("examplemod", assembly, "en-US");
        Equal(257, callback.Split('\n').Length);
        BunnyLocalizationRuntime.Register("Mods.examplemod.Common.Last", "en-US", "Last");
        Reject(() => BunnyLocalizationRuntime.Register("Mods.examplemod.Common.Overflow", "en-US", "Overflow"));
        string wire = BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US");
        Equal(258, wire.Split('\n').Length);
        Equal(false, wire.Contains(Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes(
            "Mods.examplemod.Common.Overflow")), StringComparison.Ordinal));
        Equal(false, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", callback));
        Equal(true, BunnyLocalizationRuntime.Acknowledge("examplemod", assembly, "en-US", wire));
        Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("examplemod", assembly, "en-US"));
    }
    finally
    {
        HostLocalizationModule.SeedCallbackLimit = false;
        BunnyLocalizationRuntime.Unload("examplemod");
    }
});

Test("runtime-only-register-activates-without-module-or-json", () =>
{
    string root = Path.Combine(Path.GetTempPath(), "bunny-live-only-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(Path.Combine(root, "content"));
    try
    {
        string diagnostics = Path.Combine(root, "diagnostics.txt");
        string assembly = typeof(BunnyLoader.LocalizationRuntimeOnlyProbe.RuntimeOnlyLocaleItem)
            .Assembly.Location;
        Equal(1, BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(assembly, diagnostics, root));
        string[] wire = File.ReadAllLines(Path.Combine(root, "source-runtime-values.txt"));
        Equal(true, wire.Contains("LOCALE_RUNTIME\t1"));
        Equal(false, wire.Any(row => row.StartsWith("LOCALE_MODULES\t", StringComparison.Ordinal)));
        Equal(false, wire.Any(row => row.StartsWith("LOCALE\t", StringComparison.Ordinal)));
        string first = BunnyLocalizationRuntime.Dispatch("runtimeprobe", assembly, "en-US");
        Equal("OK|0\nBUNNY_DYNAMIC_LOCALES_V1", first);
        Equal(true, BunnyLocalizationRuntime.Acknowledge("runtimeprobe", assembly, "en-US", first));
        BunnyLoader.LocalizationRuntimeOnlyProbe.RuntimeOnlyLocaleItem.InstallTranslation();
        string live = BunnyLocalizationRuntime.Drain("runtimeprobe", assembly, "en-US");
        Equal("OK|0", live.Split('\n')[0]);
        Equal(true, live.Contains(Convert.ToHexString(
            System.Text.Encoding.UTF8.GetBytes("Runtime only")), StringComparison.Ordinal));
        Equal(true, BunnyLocalizationRuntime.Acknowledge("runtimeprobe", assembly, "en-US", live));
        Equal("UNCHANGED", BunnyLocalizationRuntime.Drain("runtimeprobe", assembly, "en-US"));
        Equal(true, BunnyLocalizationRuntime.Unload("runtimeprobe"));
    }
    finally
    {
        Directory.Delete(root, true);
    }
});

Console.WriteLine("LOCALIZATION_HOST_PASS cases=" + passed);

public sealed class HostLocalizationModule : BunnyLocalizationModule
{
    public static int CallbackCount;
    public static bool ShouldFail;
    public static bool DynamicTranslations;
    public static bool RegisterForeign;
    public static bool GlobalRegistration;
    public static bool SeedCallbackLimit;
    public static string LastCulture = "";

    public override void OnLocalizationsLoaded()
    {
        CallbackCount++;
        LastCulture = BunnyLocalizationRuntime.ActiveCultureName;
        if (DynamicTranslations)
            BunnyLocalizationRuntime.Register("Mods.examplemod.Common.RuntimeGreeting",
                new Dictionary<string, string>
                {
                    ["en-US"] = "Hello at runtime",
                    ["pt-BR"] = "Olá em tempo de execução"
                });
        if (SeedCallbackLimit)
            for (int i = 0; i < 255; i++)
                BunnyLocalizationRuntime.Register("Mods.examplemod.Common.Seed" +
                    i.ToString("D3"), "en-US", "Seed");
        if (GlobalRegistration)
            BunnyLocalizationRuntime.Register("LegacyInterface.28", "pt-BR", "Menu de teste");
        if (RegisterForeign)
            BunnyLocalizationRuntime.Register("Mods.other.Common.Forbidden", "en-US", "No");
        if (ShouldFail) throw new InvalidOperationException("Callback test fault");
    }

    public override void RegisterTranslations(BunnyLocalizationCatalog catalog)
    {
        if (Environment.GetEnvironmentVariable("BUNNY_TEST_LOCALE_ONLY") == "1") return;
        if (Environment.GetEnvironmentVariable("BUNNY_TEST_GLOBAL_LOCALE") == "1")
            catalog.Register("LegacyInterface.28", "en-US", "Example menu");
        catalog.Register("Mods.source.ItemName.HostModule",
            new Dictionary<string, string>
            {
                ["en-US"] = "Host module",
                ["pt-BR"] = "Módulo de exemplo"
            });
    }
}
