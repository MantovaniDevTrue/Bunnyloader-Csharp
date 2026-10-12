using System;

namespace BunnyLoader.Source;

/// <summary>A value copy from the hit frame, not a live Terraria NPC.</summary>
public readonly struct BunnyNpcHitSnapshot
{
    public int Index { get; }
    public int Type { get; }
    public int Life { get; }
    public float X { get; }
    public float Y { get; }

    internal BunnyNpcHitSnapshot(int index, int type, int life, float x, float y)
    { Index = index; Type = type; Life = life; X = x; Y = y; }
}

/// <summary>A value copy of the projectile owner's player at the hit frame.</summary>
public readonly struct BunnyPlayerHitSnapshot
{
    public int Index { get; }
    public int Life { get; }
    public int MaxLife { get; }
    public float X { get; }
    public float Y { get; }
    /// <summary>Health fraction in [0,1]; zero when MaxLife is unavailable.</summary>
    public float HealthFraction => MaxLife <= 0 ? 0f : Math.Clamp((float)Life / MaxLife, 0f, 1f);

    internal BunnyPlayerHitSnapshot(int index, int life, int maxLife, float x, float y)
    { Index = index; Life = life; MaxLife = maxLife; X = x; Y = y; }
}

/// <summary>Immutable V4 values. Call is the shared session sequence, not an entity ID.</summary>
public readonly struct BunnyNpcHitContext
{
    public int Call { get; }
    public int Damage { get; }
    public BunnyNpcHitSnapshot Npc { get; }
    public BunnyPlayerHitSnapshot Player { get; }

    private BunnyNpcHitContext(int call, int damage, BunnyNpcHitSnapshot npc,
                              BunnyPlayerHitSnapshot player)
    { Call = call; Damage = damage; Npc = npc; Player = player; }

    /// <summary>Adapt the existing OnHitNPCV4 ABI without accessing game objects.</summary>
    public static BunnyNpcHitContext FromV4(int call, int damage,
        int npcIndex, int npcType, int npcLife, float npcX, float npcY,
        int playerIndex, int playerLife, int playerLifeMax, float playerX, float playerY)
        => new BunnyNpcHitContext(call, damage,
            new BunnyNpcHitSnapshot(npcIndex, npcType, npcLife, npcX, npcY),
            new BunnyPlayerHitSnapshot(playerIndex, playerLife, playerLifeMax, playerX, playerY));
}

/// <summary>The vanilla NPC debuffs supported by the experimental .85 protocol.</summary>
public enum BunnyNpcDebuff
{
    OnFire = 24,
    CursedInferno = 39,
    Frostburn = 44,
    Ichor = 69
}

/// <summary>
/// One optional request returned by OnHitNPCV4. It does not immediately apply a buff.
/// The game thread may reject it due to capacity, world/target changes or expiry.
/// </summary>
public readonly struct BunnyNpcBuffRequest
{
    public const int MinDurationTicks = 30;
    public const int MaxDurationTicks = 600;
    public static BunnyNpcBuffRequest None => default;
    public BunnyNpcDebuff Debuff { get; }
    public int DurationTicks { get; }
    public bool HasEffect => DurationTicks != 0;

    private BunnyNpcBuffRequest(BunnyNpcDebuff debuff, int durationTicks)
    { Debuff = debuff; DurationTicks = durationTicks; }

    public static bool TryCreate(BunnyNpcDebuff debuff, int durationTicks,
                                 out BunnyNpcBuffRequest request)
    {
        request = None;
        if (!(debuff == BunnyNpcDebuff.OnFire || debuff == BunnyNpcDebuff.CursedInferno ||
              debuff == BunnyNpcDebuff.Frostburn || debuff == BunnyNpcDebuff.Ichor) ||
            durationTicks < MinDurationTicks || durationTicks > MaxDurationTicks)
            return false;
        request = new BunnyNpcBuffRequest(debuff, durationTicks);
        return true;
    }

    public static BunnyNpcBuffRequest Create(BunnyNpcDebuff debuff, int durationTicks)
    {
        if (!(debuff == BunnyNpcDebuff.OnFire || debuff == BunnyNpcDebuff.CursedInferno ||
              debuff == BunnyNpcDebuff.Frostburn || debuff == BunnyNpcDebuff.Ichor))
            throw new ArgumentOutOfRangeException(nameof(debuff));
        if (durationTicks < MinDurationTicks || durationTicks > MaxDurationTicks)
            throw new ArgumentOutOfRangeException(nameof(durationTicks));
        return new BunnyNpcBuffRequest(debuff, durationTicks);
    }

    /// <summary>Return this only from OnHitNPCV4; other callbacks do not dispatch effects.</summary>
    public int ToCallbackResult() => HasEffect ? 1000000 + (int)Debuff * 1000 + DurationTicks : 0;
}
