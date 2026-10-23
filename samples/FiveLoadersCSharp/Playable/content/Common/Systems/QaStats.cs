using BunnyLoader.Source;

namespace FiveLoadersCSharp;

public static class QaStats
{
    public static float Damage()
    {
        // (10 + Base 2) * Additive 1.5 * Multiplicative 2 + Flat 4 = 40.
        // O utilitário calcula números; não inventa DamageClass no Mobile.
        var modifier = new StatModifier { Base = 2, Flat = 4 };
        modifier += 0.5f;
        modifier *= 2f;
        return modifier.ApplyTo(10);
    }
}
