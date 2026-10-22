using BunnyLoader.Source;

namespace ExampleModCSharp.CloudExamples;

/// <summary>
/// Exemplo baseado na nuvem comum carregada apenas pelo PNG no ExampleModBunny.
/// Quando o CloudLoader C# estiver integrado, seu peso será 1 contra as 22
/// nuvens comuns do Terraria. Esta classe é APENAS um teste de contrato por ora.
/// </summary>
public sealed class ExampleCommonCloud : BunnyCloud
{
    public override void SetDefaults()
    {
        Texture = "Content/Clouds/ExampleCloud";
    }

    public override float SpawnChance(BunnyCloudEnvironment environment) => 1f;
}
