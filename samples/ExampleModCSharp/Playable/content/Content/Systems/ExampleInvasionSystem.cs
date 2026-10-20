using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleInvasionSystem : BunnyInvasionSystem
{
    public override BunnyInvasionSpawnPipeline Pipeline => ExampleInvasion.Pipeline;
}
