using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Achievements;

public sealed class ManyWormsKilled : BunnyAchievement
{
    private const int GiantWormHead = 10;
    private BunnyAchievementCondition? kills;

    public override BunnyAchievementCategory Category
        => BunnyAchievementCategory.Collector;

    public override bool Hidden => kills == null || kills.Value == 0;

    public override void SetStaticDefaults()
    {
        FriendlyName = "Cinco vermes gigantes";
        Description = "Derrote cinco Vermes Gigantes.";
        Texture = "Content/Achievements/ManyWormsKilled.png";

        // O contador é mantido em C# e só publica mudanças ao MonoVM.
        // O estado oculto usa ??? até que o primeiro verme seja derrotado.
        kills = AddIntCondition("GiantWormsKilled", 5);
    }

    public override void OnNPCKilled(int npcType)
    {
        if (npcType == GiantWormHead)
            kills!.Increase();
    }
}
