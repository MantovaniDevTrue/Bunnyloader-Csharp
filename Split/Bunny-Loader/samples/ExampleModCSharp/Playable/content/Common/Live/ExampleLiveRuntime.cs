namespace BunnyLoader.Source;

public static class BunnyLiveEntry
{
    private static int calls;

    public static int Tick(int tick, int state)
    {
        calls++;
        return state + (tick % 7) - 3 + calls;
    }
}
