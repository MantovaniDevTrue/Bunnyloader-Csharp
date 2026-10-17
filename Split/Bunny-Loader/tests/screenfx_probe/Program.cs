using System;
using System.IO;
using BunnyLoader.SourceCompiler;
using ScreenFxProbeCSharp;
using LightsAndShadowsCSharp;

var first = new ProbeTint();
first.SetDefaults();
if (first.ShaderFile != "Effects/ProbeTint.frag" || first.Layer != 0 || first.Intensity != 1f)
    throw new InvalidOperationException("ProbeTint settings mismatch");

var second = new ProbeVignette();
second.SetDefaults();
if (second.ShaderFile != "Effects/ProbeVignette.frag" || second.Order != 10)
    throw new InvalidOperationException("ProbeVignette settings mismatch");

var lights = new LightsAndShadows();
lights.SetDefaults();
if (lights.PipelineFile != "Effects/Lights.screenfx" || lights.ShaderFile.Length != 0)
    throw new InvalidOperationException("Lights And Shadows graph declaration mismatch");
if (lights.Uniform_DayShadowThreshold != .98f ||
    lights.Uniform_NightShadowThreshold != .02f ||
    lights.Uniform_RayQuality != 3f ||
    lights.Uniform_BloomStrength != .55f)
    throw new InvalidOperationException("Managed Lights C# scene settings mismatch");

string root = Path.Combine(Path.GetTempPath(), "bunny_screenfx_verify_" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
try
{
    string diagnostics = Path.Combine(root, "probe.txt");
    int result = CompiledAssemblyProbe.Verify(typeof(ProbeTint).Assembly.Location, diagnostics);
    if (result != 3)
        throw new InvalidOperationException("Expected three managed screen effect classes: actual=" + result +
            " diagnostics=" + File.ReadAllText(diagnostics));
    if (!File.ReadAllText(diagnostics).Contains("screenEffects=3"))
        throw new InvalidOperationException("Managed verification omitted the screen effects");
    Console.WriteLine("SCREENFX_MANAGED_PROBE_OK count=3");
}
finally
{
    Directory.Delete(root, recursive: true);
}
