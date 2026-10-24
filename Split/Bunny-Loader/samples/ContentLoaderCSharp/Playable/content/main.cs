using BunnyLoader.Source;

namespace ManagedCatalogExample;

// Este mod contém APENAS serviços/dados. Não inventa item/NPC para disparar o MonoVM.
// O host identifica a categoria, compila a assembly e carrega os serviços automaticamente
// no worker persistente, antes de liberar a entrada no mundo.
public abstract class CatalogRecord : ModType
{
    public int Type { get; private set; }
    protected sealed override void Register() => Type = LoaderManager.Get<CatalogLoader>().Register(this);
    public sealed override void SetupContent() => SetStaticDefaults();
}

public sealed class CatalogLoader : Loader<CatalogRecord>
{
    // IDs deste catálogo gerenciado. Não correspondem a ItemID/NPCID do Terraria.
    public CatalogLoader() => Initialize(0);
}

[LegacyName("OldServiceSettings")]
public sealed class ServiceSettings : CatalogRecord
{
    public int BaseLife { get; private set; }
    public override void Load() => BaseLife = 120;
    public override void Unload() => BaseLife = 0;
}

public sealed class ServiceSummary : CatalogRecord
{
    public int ComputedLife { get; private set; }
    public override void SetStaticDefaults()
    {
        // Os dois serviços já foram registrados neste momento. Referenciar outro serviço
        // no construtor/Load dependeria da ordem das classes; fazê-lo aqui é determinístico.
        var current = ModContent.GetInstance<ServiceSettings>();
        var legacy = ModContent.Find<CatalogRecord>(Mod!.Name, "OldServiceSettings");
        if (!object.ReferenceEquals(current, legacy))
            throw new System.InvalidOperationException("Alias e singleton precisam apontar ao mesmo objeto.");
        ComputedLife = current!.BaseLife * 2;
    }
    // Unload acontece no descarregamento do mod/encerramento do host, NÃO ao trocar de mundo.
    public override void Unload() => ComputedLife = 0;
}
