using System.Collections.Generic;
using BunnyLoader.Source;

namespace ExampleModCSharp.Common.Localization;

public sealed class ExampleLocalizationModule : BunnyLocalizationModule
{
    public string LastAppliedCulture { get; private set; } = "";

    public override void OnLocalizationsLoaded()
    {
        LastAppliedCulture = BunnyLocalizationRuntime.ActiveCultureName;
    }

    public override void RegisterTranslations(BunnyLocalizationCatalog catalog)
    {
        catalog.Register("Mods.source.Common.Localization.ModuleGreeting",
            new Dictionary<string, string>
            {
                ["en-US"] = "Hello from C#",
                ["pt-BR"] = "Olá do C#"
            });
    }
}
