namespace BunnyLoader.Source;

public static class BunnyLiveEvents
{
    private static int hits;
    private static int kills;

    public static int OnHitNPC(int call, int damage)
    {
        hits++;
        return damage + (call % 7) + hits;
    }

    public static int OnKill(int call, int damage)
    {
        kills++;
        return damage + (call % 5) + kills;
    }
}
