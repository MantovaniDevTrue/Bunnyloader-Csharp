using System;
using BunnyLoader.Source;

namespace ContentLoaderExample;

// Uma categoria gerenciada própria. A organização segue o tModLoader:
// cada categoria registra seus objetos no Loader<T> e em ModTypeLookup<T>.
// Estes IDs identificam dados deste exemplo; NÃO são IDs de Item/NPC do Terraria.
public abstract class ExampleDefinition : ModType
{
    public int Type { get; private set; }
    public sealed override void SetupContent() => SetStaticDefaults();
    protected sealed override void Register() => Type = LoaderManager.Get<ExampleDefinitionLoader>().Register(this);
}

public sealed class ExampleDefinitionLoader : Loader<ExampleDefinition>
{
    // Quatro posições reservadas pelo consumidor deste catálogo, para demonstrar VanillaCount.
    public ExampleDefinitionLoader() => Initialize(4);
}

[LegacyName("OldDifficultySettings")]
public sealed class ExampleDifficultySettings : ExampleDefinition
{
    public int BaseLife { get; private set; }
    public override void Load() => BaseLife = 120;
    public override void Unload() => BaseLife = 0;
}

public sealed class ExampleSpawnSettings : ExampleDefinition
{
    public int Life { get; private set; }
    public override void SetStaticDefaults()
    {
        // Aqui todos os objetos DESTE mod já foram registrados. Fazer esta consulta
        // num construtor ou em Load dependeria da ordem de descoberta das classes.
        var difficulty = ModContent.Find<ExampleDefinition>(Mod!.Name, nameof(ExampleDifficultySettings));
        Life = ((ExampleDifficultySettings)difficulty).BaseLife * 2;
    }
    public override void Unload() => Life = 0;
}

public static class ExampleBootstrap
{
    public const string ModUid = "example-content-catalog";

    // Chame no bootstrap gerenciado do host/mod; este método não é um hook Android implícito.
    // Na integração com um host real, quem carrega a assembly também deve chamar Unload
    // ao desativar o mod. Nunca executar descoberta de tipos a cada frame de AI ou draw.
    public static BunnyContentMod Load()
    {
        LoaderManager.AutoLoad(typeof(ExampleBootstrap).Assembly);
        return BunnyContentLoader.Load(ModUid, typeof(ExampleBootstrap).Assembly);
    }

    public static int ReadConfiguredLife()
    {
        // Singleton refere-se ao tipo CLR EXATO. Se duas instâncias deste tipo forem
        // registradas, GetInstance retorna null e ContentInstance<T>.Instances contém ambas.
        return ModContent.GetInstance<ExampleSpawnSettings>()?.Life
            ?? throw new InvalidOperationException("Carregue as definições antes de consultar a vida.");
    }

    public static bool TryReadOptionalSettings(string dependencyUid, out ExampleDefinition? settings)
    {
        // Dependência opcional: não inventar um registro/ID quando o outro mod não está carregado.
        return ModContent.TryFind(dependencyUid, "OptionalSettings", out settings);
    }

    public static void Unload() => BunnyContentLoader.Unload(ModUid);
}
