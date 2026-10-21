using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Achievements;

public sealed class ExampleBossKilled : BunnyAchievement
{
    private const int KingSlime = 50;

    public override BunnyAchievementCategory Category
        => BunnyAchievementCategory.Slayer;

    public override void SetStaticDefaults()
    {
        FriendlyName = "Rei da geleia";
        Description = "Derrote o Rei Slime.";
        Texture = "Content/Achievements/ExampleBossKilled.png";

        // O ExampleModBunny usa o boss próprio ExampleBoss.
        // Este exemplo C# utiliza o NPC vanilla KingSlime (50) para ser
        // reproduzível mesmo quando o port ainda não possui ExampleBoss.
        AddNPCKilledCondition(KingSlime);
    }
}
