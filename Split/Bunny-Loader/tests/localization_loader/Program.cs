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

Console.WriteLine("LOCALIZATION_HOST_PASS cases=" + passed);
