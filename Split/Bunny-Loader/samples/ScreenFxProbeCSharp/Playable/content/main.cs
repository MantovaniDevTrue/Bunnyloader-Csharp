using BunnyLoader.Source;

namespace ScreenFxProbeCSharp;

public sealed class ProbeTint : BunnyScreenEffect
{
    public override void SetDefaults()
    {
        DisplayName = "CSharp Native Color Probe";
        ShaderFile = "Effects/ProbeTint.frag";
        Layer = 0;
        Order = 0;
        Intensity = 1f;
    }
}

public sealed class ProbeVignette : BunnyScreenEffect
{
    public override void SetDefaults()
    {
        DisplayName = "CSharp Native Vignette Probe";
        ShaderFile = "Effects/ProbeVignette.frag";
        Layer = 0;
        Order = 10;
        Intensity = 1f;
    }
}
