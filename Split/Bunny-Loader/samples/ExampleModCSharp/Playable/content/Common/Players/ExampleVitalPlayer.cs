using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleVitalPlayer : BunnyPlayer
{
    public int HitsTaken { get; private set; }
    public int LastDamageTaken { get; private set; }
    public int DodgeAttempts { get; private set; }
    public int DodgesPerformed { get; private set; }

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
    }
}
