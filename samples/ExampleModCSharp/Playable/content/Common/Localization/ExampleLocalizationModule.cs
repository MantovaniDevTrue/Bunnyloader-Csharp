using System.Collections.Generic;
using BunnyLoader.Source;

namespace ExampleModCSharp.Common.Localization;

public sealed class ExampleLocalizationModule : BunnyLocalizationModule
{
    public string LastAppliedCulture { get; private set; } = "";

    public override void OnLocalizationsLoaded()
    {
        LastAppliedCulture = BunnyLocalizationRuntime.ActiveCultureName;
        // O runtime usa o UID real. O nome legível do manifesto não identifica
        // o proprietário de uma chave durante callbacks de localização.
        BunnyLocalizationRuntime.Register("Mods." + BunnyLocalizationRuntime.ActiveModId +
            ".Common.Localization.RuntimeGreeting",
            new Dictionary<string, string>
            {
                ["en-US"] = "Hello during runtime",
                ["pt-BR"] = "Olá durante a execução"
            });
    }

    public override void RegisterTranslations(BunnyLocalizationCatalog catalog)
    {
        // No registro inicial, source é substituído pelo UID pelo núcleo nativo.
        catalog.Register("Mods.source.Common.Localization.ModuleGreeting",
            new Dictionary<string, string>
            {
                ["en-US"] = "Hello from C#",
                ["pt-BR"] = "Olá do C#"
            });
    }
}
