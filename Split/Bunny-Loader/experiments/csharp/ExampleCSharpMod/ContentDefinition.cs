using BunnyLoader.CSharp.Authoring;

namespace ExampleCSharpMod;

public sealed class ContentDefinition : BunnyContentModule
{
    public override BunnyModInfo Info => new(
        "7f1d6115-6c2c-4a86-bb1e-5ca7d9a21f10",
        "examplecsharpmod",
        "Example C# Mod",
        "0.1.7",
        "Bunny Loader C# Test",
        "Exemplo 100% escrito em C#: armas, armaduras, acessórios, buffs, poções, NPCs e drops.");

    public override void Register(BunnyContentRegistry content)
    {
        content.Add(new BunnyProjectile("NativeHomingBolt"));
        content.Add(new BunnyProjectile("NativePredictiveBolt"));
        content.Add(new BunnyProjectile("NativeWaveBolt"));
        content.Add(new BunnyProjectile("NativeSlimeOrb")
        {
            Width = 12,
            Height = 12,
            TimeLeft = 160,
            Friendly = false,
            Hostile = true,
            Ranged = false
        });

        content.Add(new BunnyWeapon("NativeHomingWand", "NativeHomingBolt")
        {
            ShootSpeed = 13f
        });
        content.Add(new BunnyWeapon("NativePredictiveWand", "NativePredictiveBolt")
        {
            ShootSpeed = 15f
        });
        content.Add(new BunnyWeapon("NativeWaveWand", "NativeWaveBolt")
        {
            ShootSpeed = 14f
        });

        content.Add(new BunnyNpc("NativePhaseSlime")
        {
            CombatProjectile = "NativeSlimeOrb"
        });
        content.Add(new BunnySummonItem("NativePhaseSlimeBeacon", "NativePhaseSlime"));

        content.Add(new BunnyAccessory("NativeStatCharm")
        {
            DamagePercent = 10
        });

        content.Add(new BunnyArmor("NativeArmorHelmet", BunnyArmorSlot.Head)
        {
            Defense = 5
        });
        content.Add(new BunnyArmor("NativeArmorBreastplate", BunnyArmorSlot.Body)
        {
            Defense = 6,
            ManaBonus = 20,
            MinionBonus = 1
        });
        content.Add(new BunnyArmor("NativeArmorLeggings", BunnyArmorSlot.Legs)
        {
            Defense = 5,
            MoveSpeedPercent = 5
        });
        content.Add(new BunnyArmorSet(
            "NativeArmorHelmet", "NativeArmorBreastplate", "NativeArmorLeggings")
        {
            DamagePercent = 20
        });

        content.Add(new BunnyBuff("NativeDefenseBuff")
        {
            DefenseBonus = 10
        });
        content.Add(new BunnyBuffPotion("NativeDefensePotion", "NativeDefenseBuff")
        {
            DurationTicks = 3600
        });
        content.Add(new BunnyMaterial("NativeSlimeShard"));
        content.Add(new BunnyNpcDrop("NativePhaseSlime", "NativeSlimeShard")
        {
            ChanceDenominator = 2,
            MinStack = 1,
            MaxStack = 2
        });
    }
}
