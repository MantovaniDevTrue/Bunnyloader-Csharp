namespace BunnyLoader.Source;

// Drop-in replacement for this callback file in ExampleModCSharp; not a complete .bmod.
public static class ExampleWaveBoltLiveEvents
{
    public static int OnHitNPCV4(int call, int damage, int npcIndex, int npcType,
        int npcLife, float npcX, float npcY, int playerIndex, int playerLife,
        int playerLifeMax, float playerX, float playerY)
    {
        var hit = BunnyNpcHitContext.FromV4(call, damage, npcIndex, npcType,
            npcLife, npcX, npcY, playerIndex, playerLife, playerLifeMax, playerX, playerY);
        return ChooseDebuff(hit).ToCallbackResult();
    }

    public static BunnyNpcBuffRequest ChooseDebuff(BunnyNpcHitContext hit)
    {
        var debuff = hit.Player.HealthFraction < 0.35f
            ? BunnyNpcDebuff.CursedInferno : BunnyNpcDebuff.Frostburn;
        return BunnyNpcBuffRequest.Create(debuff, hit.Npc.Life > 100 ? 240 : 120);
    }

    // Retained for loader discovery and for hits where V4 snapshots are unavailable.
    public static int OnHitNPC(int call, int damage) => 0;
    public static int OnKill(int call, int damage) => 0;
}
