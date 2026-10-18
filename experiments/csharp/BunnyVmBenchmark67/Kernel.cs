namespace BunnyLoader.VmBenchmark67;

public static class Kernel
{
    public static int Execute(int iterations, int seed)
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
