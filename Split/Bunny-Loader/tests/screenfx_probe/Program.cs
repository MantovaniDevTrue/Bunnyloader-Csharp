using System;
using System.IO;
using BunnyLoader.SourceCompiler;
using ScreenFxProbeCSharp;

var first = new ProbeTint();
first.SetDefaults();
if (first.ShaderFile != "Effects/ProbeTint.frag" || first.Layer != 0 || first.Intensity != 1f)
    throw new InvalidOperationException("ProbeTint settings mismatch");

var second = new ProbeVignette();
second.SetDefaults();
if (second.ShaderFile != "Effects/ProbeVignette.frag" || second.Order != 10)
    throw new InvalidOperationException("ProbeVignette settings mismatch");

string root = Path.Combine(Path.GetTempPath(), "bunny_screenfx_verify_" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
try
{
    string diagnostics = Path.Combine(root, "probe.txt");
    int result = CompiledAssemblyProbe.Verify(typeof(ProbeTint).Assembly.Location, diagnostics);
    if (result != 2)
        throw new InvalidOperationException("Expected two managed screen effect classes: actual=" + result +
            " diagnostics=" + File.ReadAllText(diagnostics));
    if (!File.ReadAllText(diagnostics).Contains("screenEffects=2"))
        throw new InvalidOperationException("Managed verification omitted the screen effects");
    Console.WriteLine("SCREENFX_MANAGED_PROBE_OK count=2");
}
finally
{
    Directory.Delete(root, recursive: true);
}
