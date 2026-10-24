using BunnyLoader.Source;
namespace MixedContentFixture;
public sealed class Settings : ModType
{
    public int Life { get; private set; }
    public override void Load() => Life = 77;
    protected override void Register() => ModTypeLookup<Settings>.Register(this);
    public override void Unload() => Life = 0;
}
public sealed class FixtureNpc : BunnyNPC
{
    public static int Constructions;
    public FixtureNpc() => Constructions++;
    public override void SetDefaults()
    {
        DisplayName = "Mixed fixture NPC"; Texture = "Content/NPCs/Fixture.png";
        Width = 8; Height = 8; Damage = 1; LifeMax = 10;
    }
}
