using System;
using BunnyLoader.Source;

namespace ExampleModCSharp.CloudExamples;

/// <summary>
/// Probe propositalmente falho: confirma que uma exceção de mod não
/// interrompe o desenho do céu vanilla nem desliga outra nuvem válida.
/// Nunca deve fazer parte do ExampleModCSharp distribuído.
/// </summary>
public sealed class ExampleFaultingCloud : BunnyCloud
{
    public static bool FailChance { get; set; } = true;
    public static bool FailSpawn { get; set; } = true;

    public override void SetDefaults()
    {
        Texture = "Content/Clouds/FaultingProbe";
    }

    public override float SpawnChance(BunnyCloudEnvironment environment)
    {
        if (FailChance)
            throw new InvalidOperationException("Falha simulada em SpawnChance");
        return 2f;
    }

    public override void OnSpawn(BunnyCloudSpawn cloud)
    {
        if (FailSpawn)
            throw new InvalidOperationException("Falha simulada em OnSpawn");
        cloud.SpriteEffects = BunnyCloudSpriteEffects.None;
    }
}
