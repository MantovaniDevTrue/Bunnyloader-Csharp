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

var settings = new LightingSettings();
var frame = new LightingFrame
{
    DayTime = true, GameTime = 13500, WorldSurface = 250,
    ScreenWidth = 2400, ScreenHeight = 1080,
    PlayerCenterY = 1800, MoonPhase = 0,
    SkyRed = 120, SkyGreen = 140, SkyBlue = 170
};
var day = LightingCalculator.Compute(in frame, settings);
if (!day.DayTime || !day.SunEnabled || !float.IsFinite(day.SunX) ||
    !float.IsFinite(day.SunY) || day.SunOrMoonIntensity <= 0f)
    throw new InvalidOperationException("Managed PC lighting: daylight calculation failed");
frame.InvertedGravity = true;
var inverted = LightingCalculator.Compute(in frame, settings);
if (Math.Abs(day.SunY + inverted.SunY - 1f) > .0001f)
    throw new InvalidOperationException("Managed PC lighting: gravity inversion mismatch");
frame.DayTime = false;
frame.InvertedGravity = false;
frame.GameTime = 8000;
frame.MoonPhase = 4;
var night = LightingCalculator.Compute(in frame, settings);
if (night.DayTime || night.SunOrMoonIntensity <= 0 ||
    night.ShadowThreshold != .02f)
    throw new InvalidOperationException("Managed PC lighting: moon phase mismatch");
frame.DayTime = true;
frame.DesertBackground = 248;
var desert = LightingCalculator.Compute(in frame, settings);
if (desert.ShadowThreshold != 1f)
    throw new InvalidOperationException("Managed PC lighting: desert threshold mismatch");
string wire = "1|13500|250|0|0|2400|1080|1800|0|0|120|140|170|0|0|0|0|0|0|0";
string managedResult = LightingCalculator.ComputeSceneWire(wire);
string[] values = managedResult.Split(';');
if (values.Length != 6 ||
    !float.TryParse(values[0], System.Globalization.NumberStyles.Float,
        System.Globalization.CultureInfo.InvariantCulture, out float sx) ||
    !float.TryParse(values[1], System.Globalization.NumberStyles.Float,
        System.Globalization.CultureInfo.InvariantCulture, out float sy) ||
    !float.IsFinite(sx) || !float.IsFinite(sy) ||
    Math.Abs(sy + day.SunY - 1f) > .0001f || values[5] != "1")
    throw new InvalidOperationException("Managed scene Mono wire callback contract mismatch");
Console.WriteLine("LIGHTS_MANAGED_SCENE_WIRE=PASS output=" + managedResult);
Console.WriteLine("LIGHTS_MANAGED_PC_CALCULATOR=PASS day/night gravity desert");

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
