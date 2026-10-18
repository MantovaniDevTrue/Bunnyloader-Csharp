using BunnyLoader.CSharp;

namespace ExampleCSharpMod;

public sealed class HomingProjectile : IProjectileBehavior
{
    public bool Update(ref ProjectileContext context)
    {
        if (!context.TryFindClosestHostile(out NpcView target))
        {
            context.Projectile.SetTargetIndex(-1);
            return false;
        }

        context.Projectile.SetTargetIndex(target.Index);
        return context.SteerTowards(target.X, target.Y, 6f, 18f, 0.12f);
    }
}
