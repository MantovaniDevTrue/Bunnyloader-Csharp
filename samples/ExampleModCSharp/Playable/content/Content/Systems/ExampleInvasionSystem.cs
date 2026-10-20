using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleInvasionSystem : BunnyInvasionSystem
{
    public override BunnyInvasionSpawnPipeline Pipeline => ExampleInvasion.Pipeline;
    public override string RewardItemClass => nameof(ExampleItem);
    public override int RewardStack => 5;
}
