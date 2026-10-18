namespace BunnyLoader.VmGameplay69;

public static class Kernel
{
    public static int Projectile(int ticks, int seed)
    {
        unchecked
        {
            const int count = 64;
            var x = new int[count];
            var y = new int[count];
            var vx = new int[count];
            var vy = new int[count];
            for (int i = 0; i < count; i++)
            {
                x[i] = i * 13 - 320;
                y[i] = i * 7 - 180;
                vx[i] = (i % 11 - 5) * 3;
                vy[i] = (i % 9 - 4) * 2;
            }
            int hash = seed;
            for (int t = 0; t < ticks; t++)
            {
                for (int i = 0; i < count; i++)
                {
                    int tx = ((t * 7 + i * 31) & 1023) - 512;
                    int ty = ((t * 5 + i * 19) & 511) - 256;
                    vx[i] += tx > x[i] ? 2 : -2;
                    vy[i] += ty > y[i] ? 1 : -1;
                    vx[i] = vx[i] * 15 / 16;
                    vy[i] = vy[i] * 15 / 16;
                    x[i] += vx[i];
                    y[i] += vy[i];
                    if (x[i] < -512 || x[i] > 512)
                    {
                        vx[i] = -vx[i] / 2;
                        x[i] = x[i] < 0 ? -512 : 512;
                    }
                    if (y[i] < -256 || y[i] > 256)
                    {
                        vy[i] = -vy[i] / 2;
                        y[i] = y[i] < 0 ? -256 : 256;
                    }
                    hash = (hash ^ x[i] ^ (y[i] << 1) ^
                            (vx[i] << 2) ^ (vy[i] << 3)) * 16777619;
                }
            }
            return hash;
        }
    }

    public static int Npc(int ticks, int seed)
    {
        unchecked
        {
            const int count = 64;
            var x = new int[count];
            var y = new int[count];
            var vx = new int[count];
            var vy = new int[count];
            var cooldown = new int[count];
            for (int i = 0; i < count; i++)
            {
                x[i] = i * 23 - 700;
                y[i] = i * 11 - 350;
                vx[i] = i % 7 - 3;
                vy[i] = i % 5 - 2;
                cooldown[i] = 10 + i % 17;
            }
            int hash = seed;
            for (int t = 0; t < ticks; t++)
            {
                for (int i = 0; i < count; i++)
                {
                    int tx = ((t * 3 + i * 47) & 2047) - 1024;
                    int ty = ((t * 2 + i * 37) & 1023) - 512;
                    int dx = tx - x[i];
                    int dy = ty - y[i];
                    bool attacking = System.Math.Abs(dx) < 90 &&
                                     System.Math.Abs(dy) < 70;
                    int mode = attacking ? 1 : 0;
                    int force = attacking ? 2 : 1;
                    vx[i] += dx > 0 ? force : -force;
                    vy[i] += dy > 0 ? 1 : -1;
                    if (vx[i] > 12) vx[i] = 12;
                    if (vx[i] < -12) vx[i] = -12;
                    if (vy[i] > 10) vy[i] = 10;
                    if (vy[i] < -10) vy[i] = -10;
                    if ((t & 7) == 0)
                    {
                        vx[i] = vx[i] * 7 / 8;
                        vy[i] = vy[i] * 7 / 8;
                    }
                    x[i] += vx[i];
                    y[i] += vy[i];
                    if (--cooldown[i] <= 0)
                        cooldown[i] = attacking ? 30 + i % 11 : 16 + i % 13;
                    hash = (hash ^ x[i] ^ (y[i] << 1) ^
                            (cooldown[i] << 2) ^ (mode << 6)) * 16777619;
                }
            }
            return hash;
        }
    }
}
