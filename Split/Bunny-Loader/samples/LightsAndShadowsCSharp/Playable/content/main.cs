using BunnyLoader.Source;

namespace LightsAndShadowsCSharp;

public sealed class LightsAndShadows : BunnyScreenEffect
{
    public float Uniform_SunPower { get; set; }
    public float Uniform_DayShadowThreshold { get; set; }
    public float Uniform_NightShadowThreshold { get; set; }
    public float Uniform_DayShadowSoftness { get; set; }
    public float Uniform_NightShadowSoftness { get; set; }
    public float Uniform_ShadowStrength { get; set; }
    public float Uniform_RayQuality { get; set; }
    public float Uniform_BloomThreshold { get; set; }
    public float Uniform_BloomStrength { get; set; }
    public float Uniform_BloomPower { get; set; }
    public float Uniform_BlurGain { get; set; }

    public override void SetDefaults()
    {
        DisplayName = "Lights And Shadows";
        PipelineFile = "Effects/Lights.screenfx";
        Layer = 0;
        Order = 0;
        Intensity = 1f;
        Uniform_SunPower = 1f;
        Uniform_DayShadowThreshold = 0.98f;
        Uniform_NightShadowThreshold = 0.02f;
        Uniform_DayShadowSoftness = 0.012f;
        Uniform_NightShadowSoftness = 0.015f;
        Uniform_ShadowStrength = 1f;
        Uniform_RayQuality = 3f;
        Uniform_BloomThreshold = 0.70f;
        Uniform_BloomStrength = 0.55f;
        Uniform_BloomPower = 4.5f;
        Uniform_BlurGain = 1.02f;
    }
}
