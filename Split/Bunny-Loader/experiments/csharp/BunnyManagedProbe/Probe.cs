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

    public static int BenchmarkMath(int iterations, int seed)
    {
        unchecked
        {
            uint x = (uint)seed ^ 0xA5A5A5A5u;
            for (uint i = 0; i < (uint)iterations; i++)
            {
                x += i * 0x9E3779B9u;
                x = (x << 7) | (x >> 25);
                x ^= x >> 13;
                x *= 0x85EBCA6Bu;
                x ^= x << 5;
            }
            return (int)x;
        }
    }
}
