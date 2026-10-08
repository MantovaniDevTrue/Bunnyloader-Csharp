namespace BunnyCsPrototype;

public sealed class ExampleHomingProjectile : IProjectileBehavior
{
    public bool Update(ref ProjectileContext context)
    {
        if (!context.TryFindClosestHostile(out NpcView target))
        {
            context.Projectile.TargetIndex = -1;
            return false;
        }

        context.Projectile.TargetIndex = target.Index;
        return context.SteerTowards(target.X, target.Y, 6f, 18f, 0.12f);
    }
}
