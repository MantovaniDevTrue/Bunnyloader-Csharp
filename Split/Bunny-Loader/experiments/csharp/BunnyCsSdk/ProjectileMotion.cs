using System;

namespace BunnyLoader.CSharp;

public static class ProjectileMotion
{
    public static bool AccelerateTowards(ref ProjectileContext context,
                                         float x, float y,
                                         float acceleration, float maximumSpeed)
    {
        ProjectileView p = context.Projectile;
        if (!float.IsFinite(x) || !float.IsFinite(y) ||
            !float.IsFinite(acceleration) || !float.IsFinite(maximumSpeed) ||
            acceleration <= 0f || maximumSpeed <= 0f) return false;

        float dx = x - p.X;
        float dy = y - p.Y;
        float length = MathF.Sqrt(dx * dx + dy * dy);
        if (!float.IsFinite(length) || length < 0.001f) return false;

        float ax = dx / length * maximumSpeed - p.VelocityX;
        float ay = dy / length * maximumSpeed - p.VelocityY;
        float delta = MathF.Sqrt(ax * ax + ay * ay);
        if (!float.IsFinite(delta)) return false;
        if (delta > acceleration)
        {
            ax *= acceleration / delta;
            ay *= acceleration / delta;
        }

        float vx = p.VelocityX + ax;
        float vy = p.VelocityY + ay;
        float speed = MathF.Sqrt(vx * vx + vy * vy);
        if (!float.IsFinite(speed)) return false;
        if (speed > maximumSpeed)
        {
            vx *= maximumSpeed / speed;
            vy *= maximumSpeed / speed;
        }

        p.SetVelocity(vx, vy);
        if (speed > 0.001f) p.SetRotation(MathF.Atan2(vy, vx));
        return true;
    }

    public static bool ApplyDrag(ref ProjectileContext context, float retention)
    {
        if (!float.IsFinite(retention) || retention < 0f || retention > 1f) return false;
        ProjectileView p = context.Projectile;
        float vx = p.VelocityX * retention;
        float vy = p.VelocityY * retention;
        if (!float.IsFinite(vx) || !float.IsFinite(vy)) return false;
        p.SetVelocity(vx, vy);
        return true;
    }
}
