using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSoul : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Soul";
        Texture = "Content/Items/Materials/ExampleSoul";
        Rare = ItemRarityID.Pink; Value = BuyPrice(gold: 1); MaxStack = 9999;
        Behavior.CloneVanillaDefaults("SoulofSight");
        // Quatro quadros verticais, seis ticks por quadro, flutuacao e pulso nativos.
        Behavior.Animate(4, 6, animatesAsSoul: true, pulse: true, noGravity: true);
        Behavior.WorldLight(0.53f * 0.45f, 0.81f * 0.45f, 0.92f * 0.45f, scaleByEssence: true);
        Behavior.WorldAlpha(255, 255, 255, 50);
    }
}
