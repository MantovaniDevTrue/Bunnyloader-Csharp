namespace BunnyLoader.ManagedProbe;

public static class Probe
{
    public static int Run(int a, int b)
    {
        return a + b;
    }

    public static int AI(int tick, int state)
    {
        return state + (tick % 7) - 3;
    }
}
