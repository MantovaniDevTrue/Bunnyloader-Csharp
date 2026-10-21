using BunnyLoader.Source;

namespace ExampleModCSharp.Achievements;

// ExampleBossKilled do ExampleModBunny: a conquista completa quando
// o NPC registrado pelo mod morre. 1234 é somente um ID de teste;
// no jogo, o loader deverá resolver o ID registrado do NPC.
public sealed class ExampleBossKilled : BunnyAchievement
{
    public override void SetStaticDefaults()
    {
        FriendlyName = "Derrote o chefe de exemplo";
        Description = "Derrote um chefe de exemplo.";
        Texture = "Content/Achievements/ExampleBossKilled.png";
        AddNPCKilledCondition(1234);
    }
}
