using BunnyLoader.CSharp;

namespace ExampleCSharpMod;

public sealed class PredictiveHomingProjectile : IProjectileBehavior
{
    public bool Update(ref ProjectileContext context)
    {
        if (!context.TryFindClosestHostileInRange(900f, out NpcView target))
        {
            context.Projectile.SetTargetIndex(-1);
            return false;
        }

        context.Projectile.SetTargetIndex(target.Index);
        return context.SteerTowardsPredicted(target, 8f, 19f, 0.15f, 24f);
    }
}
