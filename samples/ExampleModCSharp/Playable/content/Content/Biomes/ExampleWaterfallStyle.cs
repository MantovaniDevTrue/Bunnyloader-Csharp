using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Biomes;

/// <summary>
/// Same API shape as tModLoader's ModWaterfallStyle:
/// the loader assigns Slot, Texture identifies the PNG, and the two hooks
/// are invoked only for visible waterfalls whose slot belongs to this mod.
/// </summary>
public sealed class ExampleWaterfallStyle : ModWaterfallStyle
{
    // Bunny packages use paths relative to content/, not absolute device paths.
    // This file is copied from the existing Bunny ExampleMod asset.
    public override string Texture => "Content/Biomes/ExampleWaterfallStyle.png";

    public override void SetStaticDefaults()
    {
        // Metadata only; do not touch Terraria IL2CPP on the Mono worker.
    }

    public override void ColorMultiplier(ref float r, ref float g, ref float b, float a)
    {
        // The C# contract receives float components just like tModLoader.
        // The JS Bunny loader uses 0..255 byte Refs; do not port that conversion.
        r *= 0.65f;
        g *= 0.9f;
        b *= 1.2f;
    }

    public override void AddLight(int i, int j)
    {
        // The managed callback queues this light; the native game thread
        // eventually calls Terraria's light API. No JS or Java callback.
        WaterfallLighting.AddLight(i, j, 0.1f, 0.35f, 0.6f);
    }
}
