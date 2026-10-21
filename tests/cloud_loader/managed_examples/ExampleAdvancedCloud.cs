using BunnyLoader.Source;

namespace ExampleModCSharp.CloudExamples;

/// <summary>
/// Tradução C# do comportamento de ExampleAdvancedCloud.js do ExampleModBunny.
/// O peso é 10 no bioma do mod e 1 fora dele. O texto no PNG exige que a
/// nuvem nunca nasça espelhada. Ainda não é registrável no Terraria Mobile.
/// </summary>
public sealed class ExampleAdvancedCloud : BunnyCloud
{
    public override void SetDefaults()
    {
        Texture = "Content/Clouds/ExampleAdvancedCloud";
    }

    public override bool RareCloud => true;

    public override float SpawnChance(BunnyCloudEnvironment environment) =>
        environment.InBiome("ExampleModCSharp/ExampleSurfaceBiome") ? 10f : 1f;

    public override void OnSpawn(BunnyCloudSpawn cloud)
    {
        // Copiar para o slot só após confirmar que a nuvem continua ativa.
        cloud.SpriteEffects = BunnyCloudSpriteEffects.None;
    }
}
