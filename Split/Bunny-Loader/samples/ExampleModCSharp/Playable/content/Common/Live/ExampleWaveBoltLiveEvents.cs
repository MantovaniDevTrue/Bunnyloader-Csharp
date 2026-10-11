namespace BunnyLoader.Source;

public static class ExampleWaveBoltLiveEvents
{
    private static int hits;
    private static int kills;

    public static int OnHitNPC(int call, int damage)
    {
        hits++;
        return 1000 + damage + (call % 11) + hits;
    }

    public static int OnHitNPCV2(int call, int damage, int npcIndex)
    {
        hits++;
        return 10000 + damage + (call % 11) + hits + npcIndex;
    }

    public static int OnKill(int call, int damage)
    {
        kills++;
        return 2000 + damage + (call % 13) + kills;
    }
}
