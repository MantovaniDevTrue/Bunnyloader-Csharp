using System.Reflection;
using System.Runtime.InteropServices;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;

static void Check(bool value, string label)
{
    if (!value) throw new Exception(label);
}
Check(args.Length == 2, "sample root and vector output required");
Check(!default(BunnyNpcBuffRequest).HasEffect && BunnyNpcBuffRequest.None.ToCallbackResult() == 0, "default is no effect");
using (var output = new StreamWriter(args[1]))
{
    foreach (var buff in Enum.GetValues<BunnyNpcDebuff>())
    for (int duration = 30; duration <= 600; ++duration)
    {
        Check(BunnyNpcBuffRequest.TryCreate(buff, duration, out var request), "valid request");
        Check(request.Debuff == buff && request.DurationTicks == duration, "request values");
        Check(request.ToCallbackResult() == BunnyNpcBuffRequest.Create(buff, duration).ToCallbackResult(), "factory parity");
        output.WriteLine($"{request.ToCallbackResult()} {(int)buff} {duration}");
    }
}
foreach (int buff in new[] { int.MinValue, -1, 0, 23, 25, 40, 70, int.MaxValue })
    Check(!BunnyNpcBuffRequest.TryCreate((BunnyNpcDebuff)buff, 120, out var invalid) && invalid.ToCallbackResult() == 0, "invalid buff");
foreach (int duration in new[] { int.MinValue, -1, 0, 29, 601, 1000, int.MaxValue })
    Check(!BunnyNpcBuffRequest.TryCreate(BunnyNpcDebuff.OnFire, duration, out var invalid) && !invalid.HasEffect, "invalid duration");
try { BunnyNpcBuffRequest.Create((BunnyNpcDebuff)0, 120); throw new Exception("missing buff validation"); }
catch (ArgumentOutOfRangeException e) { Check(e.ParamName == "debuff", "buff argument name"); }
try { BunnyNpcBuffRequest.Create(BunnyNpcDebuff.Ichor, 601); throw new Exception("missing duration validation"); }
catch (ArgumentOutOfRangeException e) { Check(e.ParamName == "durationTicks", "duration argument name"); }
var hit = BunnyNpcHitContext.FromV4(7, 55, 9, 123, 450, 10.5f, -20.25f, 2, 100, 400, 33.5f, 88.25f);
Check(hit.Call == 7 && hit.Damage == 55 && hit.Npc.Index == 9 && hit.Npc.Type == 123 && hit.Npc.Life == 450 && hit.Npc.X == 10.5f && hit.Npc.Y == -20.25f, "NPC context");
Check(hit.Player.Index == 2 && hit.Player.Life == 100 && hit.Player.MaxLife == 400 && hit.Player.HealthFraction == 0.25f && hit.Player.X == 33.5f && hit.Player.Y == 88.25f, "player context");
Check(default(BunnyPlayerHitSnapshot).HealthFraction == 0, "default player");
Check(BunnyNpcHitContext.FromV4(0,0,0,0,1,0,0,0,int.MaxValue,int.MaxValue,0,0).Player.HealthFraction == 1f, "no integer overflow");

// Use the actual compiler used by the Android worker, not a separate sample build.
string temporary = Path.Combine(Path.GetTempPath(), "bunny-live-api-" + Guid.NewGuid());
Directory.CreateDirectory(temporary);
try
{
    string framework = Path.Combine(temporary, "framework");
    Directory.CreateDirectory(framework);
    foreach (string file in Directory.GetFiles(RuntimeEnvironment.GetRuntimeDirectory(), "*.dll"))
        File.Copy(file, Path.Combine(framework, Path.GetFileName(file)));
    File.Copy(typeof(BunnyNpcHitContext).Assembly.Location, Path.Combine(framework, "BunnySourceSdk.dll"));
    string dll = Path.Combine(temporary, "typed-sample.dll");
    string diagnostics = Path.Combine(temporary, "diagnostics.txt");
    int compiled = Compiler.Compile(args[0], dll, framework, diagnostics);
    Check(compiled > 0 && File.Exists(dll), "Roslyn sample: " + File.ReadAllText(diagnostics));
    Check(Compiler.Compile(args[0], dll, framework, diagnostics) == 2, "SDK-aware compile cache hit");
    var klass = Assembly.LoadFile(dll).GetType("BunnyLoader.Source.ExampleWaveBoltLiveEvents")!;
    var method = klass.GetMethod("OnHitNPCV4")!;
    Check(method.GetParameters().Length == 12 && method.ReturnType == typeof(int), "V4 ABI");
    object[] values = {7,55,9,123,450,10.5f,-20.25f,2,100,400,33.5f,88.25f};
    Check((int)method.Invoke(null, values)! == BunnyNpcBuffRequest.Create(BunnyNpcDebuff.CursedInferno,240).ToCallbackResult(), "low health example");
    values[8] = 400; values[4] = 80;
    Check((int)method.Invoke(null, values)! == BunnyNpcBuffRequest.Create(BunnyNpcDebuff.Frostburn,120).ToCallbackResult(), "healthy example");
    Check((int)klass.GetMethod("OnHitNPC")!.Invoke(null, new object[]{1,20})! == 0, "legacy fallback no effect");
    Check((int)klass.GetMethod("OnKill")!.Invoke(null, new object[]{1,20})! == 0, "kill no effect");
}
finally { Directory.Delete(temporary, true); }
Console.WriteLine("TYPED_LIVE_API_AND_ROSLYN_SAMPLE=PASS");
