namespace ExampleModCSharp;

public readonly record struct BlockyBuffResult(bool KeepBuff, float JumpSpeedBoost, int ExtraFall)
{
    public bool RemoveBuff => !KeepBuff;
}

public sealed class ExampleCostumePlayer
{
    public bool BlockyAccessoryPrevious { get; private set; }
    public bool BlockyAccessory { get; private set; }
    public bool BlockyHideVanity { get; private set; }
    public bool BlockyForceVanity { get; private set; }
    public bool BlockyPower { get; private set; }

    public bool BlockyVanityEffects => BlockyForceVanity || (BlockyPower && !BlockyHideVanity);

    public void ResetEffects()
    {
        BlockyAccessoryPrevious = BlockyAccessory;
        BlockyAccessory = false;
        BlockyHideVanity = false;
        BlockyForceVanity = false;
        BlockyPower = false;
    }

    public void UpdateAccessory(bool vanity, bool hideVisual)
    {
        if (vanity)
            return;

        BlockyAccessory = true;
        BlockyHideVanity = hideVisual;
    }

    public void UpdateVanity()
    {
        BlockyHideVanity = false;
        BlockyForceVanity = true;
    }

    public bool UpdateEquips(int townNpcs)
    {
        return townNpcs >= 1 && BlockyAccessory;
    }

    public BlockyBuffResult UpdateBuff(int townNpcs)
    {
        if (townNpcs < 1 || !BlockyAccessoryPrevious)
            return default;

        BlockyPower = true;
        return new BlockyBuffResult(true, 4.8f, 45);
    }

    public string? FrameEffects(bool wet)
    {
        if (!BlockyVanityEffects)
            return null;

        return wet ? "BlockyAlt" : "ExampleCostume";
    }
}
