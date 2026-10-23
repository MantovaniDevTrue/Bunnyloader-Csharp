using BunnyLoader.Source;

namespace ExampleMod.Content.Achievements;

public sealed class ManyWormsKilled : BunnyAchievement
{
    private const int GiantWormHead = 10;
    private BunnyAchievementCondition counter = null!;

    public override BunnyAchievementCategory Category => BunnyAchievementCategory.Collector;
    public override bool Hidden => counter == null || counter.Value == 0f;

    public override void SetStaticDefaults()
    {
        counter = AddIntCondition(5);
    }

    public override void OnNPCKilled(BunnyAchievementEvent e)
    {
        if (e.IsLocalPlayer && e.ContentType == GiantWormHead)
            counter.Value++;
    }
}
