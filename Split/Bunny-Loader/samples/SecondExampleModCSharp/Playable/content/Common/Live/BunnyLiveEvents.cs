namespace BunnyLoader.Source;

public static class BunnyLiveEvents
{
    private static int hits;
    private static int kills;

    public static int OnHitNPC(int call, int damage)
    {
        hits++;
        return 3000 + damage + (call % 11) + hits;
    }

    public static int OnKill(int call, int damage)
    {
        kills++;
        return 4000 + damage + (call % 13) + kills;
    }
}
