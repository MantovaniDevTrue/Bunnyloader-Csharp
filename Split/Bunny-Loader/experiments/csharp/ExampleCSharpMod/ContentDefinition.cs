using BunnyLoader.CSharp.Authoring;

namespace ExampleCSharpMod;

public sealed class ContentDefinition : BunnyContentModule
{
    public override BunnyModInfo Info => new(
        "7f1d6115-6c2c-4a86-bb1e-5ca7d9a21f10",
        "examplecsharpmod",
        "Example C# Mod",
        "0.1.6",
        "Bunny Loader C# Test",
        "Exemplo escrito em C#: armas, projetis, NPC e receitas. NativeAOT executa a IA.");

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
    }
}
