using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleGuardianShieldPlayer : BunnyPlayer
{
    public int NpcBlocks { get; private set; }
    public int ProjectileBlocks { get; private set; }
    public int LastDamageTaken { get; private set; }

    public override void SetDefaults()
    {
        Accessory = nameof(ExampleGuardianShieldAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            DefenseBonus = 5;
    }

    public override int UpdateLifeRegen(bool accessoryEquipped)
    {
        return accessoryEquipped ? 4 : 0;
    }

    public override void ModifyHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0 ||
            modifiers.NpcIndex != npcIndex)
            return;

        modifiers.Damage = BunnyCombat.ReduceDamage(modifiers.Damage, 15);
        NpcBlocks++;
    }

    public override void ModifyHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0 ||
            modifiers.ProjectileIndex != projectileIndex)
            return;

        modifiers.Damage = BunnyCombat.ReduceDamage(modifiers.Damage, 10);
        ProjectileBlocks++;
    }

    public override void OnHurt(bool accessoryEquipped, BunnyHurtInfo info)
    {
        if (accessoryEquipped && info.Damage > 0)
            LastDamageTaken = info.Damage;
    }
}
