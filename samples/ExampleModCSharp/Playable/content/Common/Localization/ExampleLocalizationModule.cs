using System.Collections.Generic;
using System.Globalization;
using BunnyLoader.Source;
using ExampleModCSharp.Content.Items.Armor;

namespace ExampleModCSharp.Common.Localization;

// O callback é reexecutado quando o idioma muda. O loader fornece UID/idioma
// e descarta respostas de uma revisão antiga após recarga ou unload.
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
        // Reutiliza todas as traduções do JSON original, como Translate(...)
        // seguido de replace('{0}', bonus) no AddArmorSets do Bunny JS. A chave
        // formatada pertence ao mesmo UID e mantém o LocalizedText vivo no jogo.
        var armorTexts = catalog.Texts("Mods.source.ArmorSetBonus.ExampleArmor")
            ?? throw new System.InvalidOperationException("Traduções do conjunto não encontradas");
        foreach (var translation in armorTexts)
            catalog.Register("Mods.source." + ExampleHelmet.ArmorSetLocalizationKey,
                translation.Key, string.Format(CultureInfo.InvariantCulture,
                    translation.Value, ExampleHelmet.AdditiveGenericDamageBonus));
    }
}
