using ExampleModCSharp;

var player = new ExampleRandomCompatProbePlayer();
player.SetDefaults();
player.UpdateEquips(false);
const string expected = "Bunny C# Random Compat: PASS (seed/weights/bool/float/vector/list)";
for (int attempt = 0; attempt < 200; attempt++)
{
    string actual = player.OnEnterWorld();
    if (actual != expected)
        throw new InvalidOperationException($"Android probe failed on attempt {attempt}: {actual}");
}
Console.WriteLine("BUNNY_CSHARP_RANDOM_ANDROID_PROBE_PASS checks=200");
