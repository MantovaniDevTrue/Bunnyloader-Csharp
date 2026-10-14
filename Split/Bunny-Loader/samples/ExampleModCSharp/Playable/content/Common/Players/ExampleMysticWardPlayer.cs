using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMysticWardPlayer : BunnyPlayer
{
    public int LowLifeGuards { get; private set; }
    public int NearbyProjectileGuards { get; private set; }
    public int HitsObserved { get; private set; }
    public BunnyHurtSource LastHitSource { get; private set; } = BunnyHurtSource.Unknown;

    public override void SetDefaults()
    {
        Accessory = nameof(ExampleMysticWardAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            DefenseBonus = 3;
    }

    public override int UpdateManaRegen(bool accessoryEquipped)
    {
        return accessoryEquipped ? 8 : 0;
    }

    public override void ModifyHurt(bool accessoryEquipped, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0 ||
            !BunnyCombat.IsLowLife(modifiers.CurrentLife, modifiers.MaxLife, 35))
            return;

        modifiers.Damage = BunnyCombat.ReduceDamage(modifiers.Damage, 20);
        LowLifeGuards++;
    }

    public override void ModifyHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0 ||
            modifiers.ProjectileIndex != projectileIndex ||
            !BunnyCombat.IsWithinDistance(modifiers.AttackerDistance, 192f))
            return;

        modifiers.Damage = BunnyCombat.ReduceDamage(modifiers.Damage, 10);
        NearbyProjectileGuards++;
    }

    public override void OnHurt(bool accessoryEquipped, BunnyHurtInfo info)
    {
        if (!accessoryEquipped || info.Damage <= 0)
            return;

        BunnyVisual.SetPlayerOverlay(0, 1001,
            "68c3115f-62f4-49f2-a56b-388256781d5f:Content/Effects/MysticWardBarrier.png", 1.25f, 0.72f,
            0xFF66CCFF, 18);
        HitsObserved++;
        LastHitSource = BunnyCombat.SourceOf(info.NpcIndex, info.ProjectileIndex);
    }
}
