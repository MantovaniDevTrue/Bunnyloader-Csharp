using BunnyLoader.Source;

namespace LightsAndShadowsCSharp;

public sealed class LightsAndShadows : BunnyScreenEffect
{
    public override void SetDefaults()
    {
        DisplayName = "Lights And Shadows";
        PipelineFile = "Effects/Lights.screenfx";
        Layer = 0;
        Order = 0;
        Intensity = 1f;
    }
}
