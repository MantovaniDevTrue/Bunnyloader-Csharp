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
        "Split", "Bunny-Loader", "samples", "ExampleModCSharp", "Playable", "content");
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
    catalog.Register("Mods.owner.Items.Label", "en-US", "Explicit");
    var held = catalog.SnapshotForGame();
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

Console.WriteLine("LOCALIZATION_HOST_PASS cases=" + passed);
