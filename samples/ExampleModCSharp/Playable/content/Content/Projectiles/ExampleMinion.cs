using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinion : BunnyProjectile
{
    private static readonly BunnyMinionMovement movement = new(
        followSpeed: 4f, attackSpeed: 8f, catchUpSpeed: 12f,
        followInertia: 80f, attackInertia: 20f,
        followOffsetX: 10f, followOffsetY: -48f);

    public override void SetDefaults()
    {
        DisplayName = "Example Minion";
        Texture = "Content/Projectiles/ExampleMinion.png";
        Width = 18;
        Height = 28;
        Frames = 4;
        AnimationFrameTicks = 5;
        AiStyle = 0;
        Friendly = true;
        Minion = true;
        MinionContactDamage = true;
        // The native controller updates every game tick; custom C# AI remains available with null.
        // Cancelling the associated buff or losing the owner ends the real projectile lifecycle.
        MinionMovement = movement;
        MinionSlots = 1f;
        TileCollide = false;
        IgnoreWater = true;
        Penetrate = -1;
        TimeLeft = 18000;
        MinionTargetRange = 700f;
        // Mesmo brilho branco de Visuals no exemplo original; o motor emite a luz.
        NativeDefaults.SetFloat("light",0.78f);
        MinionScanTicks = 8;
    }

}
