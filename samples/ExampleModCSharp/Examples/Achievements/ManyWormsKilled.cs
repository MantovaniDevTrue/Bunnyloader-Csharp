using BunnyLoader.Source;

namespace ExampleModCSharp.Achievements;

// ManyWormsKilled do ExampleModBunny: demonstra condição numérica,
// contador feito no callback e conquista oculta até o primeiro evento.
// O número 10 é fixture, não um ID garantido para o Terraria real.
public sealed class ManyWormsKilled : BunnyAchievement
{
    private BunnyAchievementCondition? worms;

    public override BunnyAchievementCategory Category => BunnyAchievementCategory.Collector;

    public override bool Hidden => worms == null || worms.Value == 0;

    public override void SetStaticDefaults()
    {
        FriendlyName = "Cinco vermes";
        Description = "Derrote cinco vermes gigantes.";
        Texture = "Content/Achievements/ManyWormsKilled.png";
        worms = AddIntCondition("WormsKilled", 5);
    }

    public override void OnNPCKilled(int npcType)
    {
        if (npcType == 10)
            worms!.Increase();
    }
}
