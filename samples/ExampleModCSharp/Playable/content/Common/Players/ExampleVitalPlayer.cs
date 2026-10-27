// ExampleMod C# | Common/Players/ExampleVitalPlayer.cs
// BunnyPlayer: confira quais callbacks sao de preparacao e quais recebem contexto durante o jogo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleVitalPlayer : BunnyPlayer
{
    public int HitsTaken { get; private set; }
    public int LastDamageTaken { get; private set; }
    public int DodgeAttempts { get; private set; }
    public int DodgesPerformed { get; private set; }
    public int NpcHitsTaken { get; private set; }
    public int ProjectileHitsTaken { get; private set; }
    public int LastNpcIndex { get; private set; } = -1;
    public int LastProjectileIndex { get; private set; } = -1;
    public int NpcPreHitCalls { get; private set; }
    public int ProjectilePreHitCalls { get; private set; }
    public int LastNpcType { get; private set; } = -1;
    public int LastNpcDamage { get; private set; } = -1;
    public int LastNpcLife { get; private set; } = -1;
    public int LastProjectileType { get; private set; } = -1;
    public int LastProjectileDamage { get; private set; } = -1;
    public int LastProjectileOwner { get; private set; } = -1;
    public int SlimePreHits { get; private set; }
    public int EnemyProjectilePreHits { get; private set; }
    public float LastAttackerX { get; private set; } = -1f;
    public float LastAttackerY { get; private set; } = -1f;
    public float LastAttackerVelocityX { get; private set; } = -1f;
    public float LastAttackerVelocityY { get; private set; } = -1f;
    public float LastAttackerAngle { get; private set; } = -1f;
    public float LastAttackerDistance { get; private set; } = -1f;
    public float LastPostHitDistance { get; private set; } = -1f;

    public override void SetDefaults()
    {
        Accessory = nameof(ExampleVitalAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override int UpdateLifeRegen(bool accessoryEquipped)
    {
        return accessoryEquipped ? 12 : 0;
    }

    public override void ModifyHurt(bool accessoryEquipped, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0)
            return;

        modifiers.Damage = (int)((long)modifiers.Damage * 3 / 4);
    }

    public override void ModifyHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0 ||
            modifiers.NpcIndex != npcIndex)
            return;

        NpcPreHitCalls++;
        LastAttackerX = modifiers.AttackerX;
        LastAttackerY = modifiers.AttackerY;
        LastAttackerVelocityX = modifiers.AttackerVelocityX;
        LastAttackerVelocityY = modifiers.AttackerVelocityY;
        LastAttackerAngle = modifiers.AttackerAngle;
        LastAttackerDistance = modifiers.AttackerDistance;
        LastNpcType = modifiers.NpcType;
        LastNpcDamage = modifiers.NpcDamage;
        LastNpcLife = modifiers.NpcLife;
        if (modifiers.NpcType == 1)
            SlimePreHits++;
        modifiers.Damage = (int)((long)modifiers.Damage * 4 / 5);
    }

    public override void ModifyHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0 ||
            modifiers.ProjectileIndex != projectileIndex)
            return;

        ProjectilePreHitCalls++;
        LastAttackerX = modifiers.AttackerX;
        LastAttackerY = modifiers.AttackerY;
        LastAttackerVelocityX = modifiers.AttackerVelocityX;
        LastAttackerVelocityY = modifiers.AttackerVelocityY;
        LastAttackerAngle = modifiers.AttackerAngle;
        LastAttackerDistance = modifiers.AttackerDistance;
        LastProjectileType = modifiers.ProjectileType;
        LastProjectileDamage = modifiers.ProjectileDamage;
        LastProjectileOwner = modifiers.ProjectileOwner;
        if (modifiers.ProjectileOwner == 255)
            EnemyProjectilePreHits++;
        modifiers.Damage = (int)((long)modifiers.Damage * 2 / 3);
    }

    public override bool FreeDodge(bool accessoryEquipped, BunnyHurtModifiers modifiers)
    {
        if (!accessoryEquipped || modifiers.PvP || !modifiers.Dodgeable ||
            modifiers.Damage <= 0)
            return false;

        DodgeAttempts++;
        if (DodgeAttempts % 4 != 0)
            return false;

        DodgesPerformed++;
        return true;
    }

    public override void OnHurt(bool accessoryEquipped, BunnyHurtInfo info)
    {
        if (!accessoryEquipped || info.Damage <= 0)
            return;

        HitsTaken++;
        LastDamageTaken = info.Damage;
        LastPostHitDistance = info.AttackerDistance;
    }

    public override void OnHitByNPC(bool accessoryEquipped, int npcIndex, BunnyHurtInfo info)
    {
        if (!accessoryEquipped || info.Damage <= 0)
            return;

        NpcHitsTaken++;
        LastNpcIndex = npcIndex;
    }

    public override void OnHitByProjectile(bool accessoryEquipped, int projectileIndex, BunnyHurtInfo info)
    {
        if (!accessoryEquipped || info.Damage <= 0)
            return;

        ProjectileHitsTaken++;
        LastProjectileIndex = projectileIndex;
    }
}

