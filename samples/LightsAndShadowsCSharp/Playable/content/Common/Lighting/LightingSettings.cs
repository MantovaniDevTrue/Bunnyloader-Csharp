namespace LightsAndShadowsCSharp;

public sealed class LightingSettings
{
    public bool UseLight { get; set; } = true;
    public float LightIntensity { get; set; } = 1f;
    public float MoonlightIntensity { get; set; } = 1f;
    public float ShadowIntensity { get; set; } = 1f;
    public bool AlternateDaylight { get; set; }
    public bool UseBloom { get; set; } = true;
    public float BloomIntensity { get; set; } = 1f;
    public int BloomPasses { get; set; } = 4;
    public int RayQuality { get; set; } = 3;

    public void Validate()
    {
        LightIntensity = Clamp(LightIntensity, 0.4f, 1.5f);
        MoonlightIntensity = Clamp(MoonlightIntensity, 0.4f, 1.1f);
        ShadowIntensity = Clamp(ShadowIntensity, 0.4f, 1.5f);
        BloomIntensity = Clamp(BloomIntensity, 0.2f, 1.25f);
        BloomPasses = BloomPasses < 1 ? 1 : BloomPasses > 10 ? 10 : BloomPasses;
        RayQuality = RayQuality < 1 ? 1 : RayQuality > 8 ? 8 : RayQuality;
    }

    public float BloomThreshold => 0.71f - BloomIntensity * 0.01f;
    public float KawaseGain => 1f + BloomPasses * 0.005f;
    public string DaylightTexture => AlternateDaylight ? "ColorTex_Alt.png" : "ColorTex.png";

    private static float Clamp(float value, float min, float max)
    {
        return value < min ? min : value > max ? max : value;
    }
}
