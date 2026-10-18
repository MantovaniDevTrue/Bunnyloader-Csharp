using System;

namespace BunnyLoader.Source;

/// <summary>Immutable movement settings. Reuse one instance; no settings allocation per AI tick.</summary>
public sealed class BunnyMinionMovement
{
    public static BunnyMinionMovement Default { get; } = new();
    public float FollowSpeed { get; }
    public float AttackSpeed { get; }
    public float CatchUpSpeed { get; }
    public float FollowInertia { get; }
    public float AttackInertia { get; }
    public float FollowOffsetX { get; }
    public float FollowOffsetY { get; }
    public float CatchUpDistance { get; }
    public float IdleDistance { get; }
    public float IdleDamping { get; }
    public float CatchUpInertia { get; }
    public float TeleportDistance { get; }
    public float FormationSpacing { get; }
    public float AttackStopDistance { get; }
    public float OwnerLeashDistance { get; }
    public float RotationFactor { get; }
    public float Separation { get; }
    public BunnyMinionMovement(float followSpeed=6f,float attackSpeed=10f,float catchUpSpeed=12f,
        float followInertia=18f,float attackInertia=12f,float followOffsetX=-42f,float followOffsetY=-46f,
        float catchUpDistance=600f,float idleDistance=20f,float idleDamping=0.82f,
        float catchUpInertia=60f,float teleportDistance=2000f,float formationSpacing=40f,
        float attackStopDistance=40f,float ownerLeashDistance=1200f,float rotationFactor=.05f,float separation=.04f)
    {
        static void Range(float value,float min,float max,string field) {
            if(!float.IsFinite(value)||value<min||value>max)
                throw new ArgumentOutOfRangeException(field,value,"[CSharp][ERROR][MinionMovement] Stage=Validate Field="+field+" Expected="+min+".."+max+" Reason=OutOfRange");
        }
        Range(followSpeed,0.01f,40f,nameof(followSpeed));Range(attackSpeed,0.01f,40f,nameof(attackSpeed));
        Range(catchUpSpeed,0.01f,40f,nameof(catchUpSpeed));Range(followInertia,1f,1000f,nameof(followInertia));
        Range(attackInertia,1f,1000f,nameof(attackInertia));Range(followOffsetX,-2000,2000,nameof(followOffsetX));
        Range(followOffsetY,-2000,2000,nameof(followOffsetY));Range(catchUpDistance,1,4000,nameof(catchUpDistance));
        Range(idleDistance,0,catchUpDistance,nameof(idleDistance));Range(idleDamping,0,1,nameof(idleDamping));
        Range(catchUpInertia,1,1000,nameof(catchUpInertia));Range(teleportDistance,catchUpDistance+.01f,10000,nameof(teleportDistance));
        Range(formationSpacing,0,200,nameof(formationSpacing));Range(attackStopDistance,0,400,nameof(attackStopDistance));
        Range(ownerLeashDistance,1,4000,nameof(ownerLeashDistance));Range(rotationFactor,-1,1,nameof(rotationFactor));Range(separation,0,1,nameof(separation));
        CatchUpInertia=catchUpInertia;TeleportDistance=teleportDistance;FormationSpacing=formationSpacing;
        AttackStopDistance=attackStopDistance;OwnerLeashDistance=ownerLeashDistance;RotationFactor=rotationFactor;Separation=separation;
        FollowSpeed=followSpeed;AttackSpeed=attackSpeed;CatchUpSpeed=catchUpSpeed;
        FollowInertia=followInertia;AttackInertia=attackInertia;FollowOffsetX=followOffsetX;FollowOffsetY=followOffsetY;
        CatchUpDistance=catchUpDistance;IdleDistance=idleDistance;IdleDamping=idleDamping;
    }
}
public static class BunnyMinionSteering
{
    public static bool FollowOrAttack(ref BunnyProjectileAiState state)=>FollowOrAttack(ref state,BunnyMinionMovement.Default);
    public static bool FollowOrAttack(ref BunnyProjectileAiState state,BunnyMinionMovement movement)
    {
        ArgumentNullException.ThrowIfNull(movement);
        if(!float.IsFinite(state.VelocityX)||!float.IsFinite(state.VelocityY))return false;
        float dx=(state.HasTarget?state.TargetX:state.PlayerX+movement.FollowOffsetX)-state.X;
        float dy=(state.HasTarget?state.TargetY:state.PlayerY+movement.FollowOffsetY)-state.Y;
        float squared=dx*dx+dy*dy;
        if(!float.IsFinite(squared))return false;
        // The idle radius includes an exact match; the old helper left stale velocity there.
        if(!state.HasTarget&&squared<=movement.IdleDistance*movement.IdleDistance) {
            state.VelocityX*=movement.IdleDamping;state.VelocityY*=movement.IdleDamping;return true;
        }
        if(squared<0.01f)return false;
        float distance=MathF.Sqrt(squared);
        float speed=state.HasTarget?movement.AttackSpeed:squared>movement.CatchUpDistance*movement.CatchUpDistance?movement.CatchUpSpeed:movement.FollowSpeed;
        float inertia=state.HasTarget?movement.AttackInertia:movement.FollowInertia;
        float vx=(state.VelocityX*(inertia-1f)+dx*(speed/distance))/inertia;
        float vy=(state.VelocityY*(inertia-1f)+dy*(speed/distance))/inertia;
        if(!float.IsFinite(vx)||!float.IsFinite(vy))return false;
        state.VelocityX=vx;state.VelocityY=vy;return true;
    }
}
