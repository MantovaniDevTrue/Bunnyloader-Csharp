using System;
using System.Collections.Generic;
using BunnyLoader.Source;

namespace CSharpWaterfallTests;

internal static class Program
{
    private static int assertions;

    private static void Check(bool condition, string message)
    {
        assertions++;
        if (!condition) throw new InvalidOperationException("FAIL: " + message);
    }

    private static T Expect<T>(Action action, string message) where T : Exception
    {
        try { action(); }
        catch (T error) { return error; }
        throw new InvalidOperationException("Expected " + typeof(T).Name + ": " + message);
    }

    public static int Main()
    {
        try
        {
            Run();
            Console.WriteLine("WaterfallStyleLoader: " + assertions + " assertions passed; NO APK.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }

    private static void Run()
    {
        var loader = new WaterfallStyleLoader();
        var first = new TestWaterfall();
        var second = new TestWaterfall();
        var sink = new CapturedLights();

        Check(loader.TotalCount == WaterfallStyleLoader.VanillaCount, "initial slot count");
        Check(loader.GetWaterfallStyle(28) is null, "unpublished content invisible");
        Check(!loader.TryColorMultiplier(28, ref sink.R, ref sink.G, ref sink.B, 1f),
            "callbacks disabled before activation");
        Expect<InvalidOperationException>(() => loader.Activate(), "cannot activate without Seal");
        Expect<ArgumentException>(() => loader.Register("mod/unsafe", first, _ => true), "reject unsafe owner");
        Expect<InvalidOperationException>(() => loader.Register("testmod", first, _ => false), "reject missing texture");
        Check(first.Slot == -1 && loader.TotalCount == 28, "bad asset cannot reserve slot");
        Check(loader.Register("testmod", new DisabledWaterfall(), _ => true) == -1, "IsLoadingEnabled");
        Check(loader.Register("testmod", first, _ => true) == 28, "first custom slot");
        Check(loader.Register("othermod", second, _ => true) == 29, "cross-mod slot determinism");
        Check(first.FullName == "testmod/" + typeof(TestWaterfall).FullName, "stable content identity");
        Check(first.LoadCount == 1 && first.StaticDefaultsCount == 1, "load and setup once");
        Expect<InvalidOperationException>(() =>
            loader.Register("testmod", new TestWaterfall(), _ => true), "duplicate name");
        Expect<InvalidOperationException>(() => loader.Register("third", first, _ => true), "instance reused");

        var descriptors = loader.Seal();
        Check(descriptors.Count == 2, "two native texture descriptors");
        Check(descriptors[0].Slot == 28 && descriptors[1].Slot == 29, "descriptor slot order");
        Check(descriptors[0].Texture == "Content/Biomes/TestWaterfall.png", "asset relative path");
        Check(loader.TotalCount == 30, "published count");
        Check(ReferenceEquals(loader.GetWaterfallStyle(28), first), "lookup by slot");
        Check(loader.GetWaterfallStyle(27) is null && loader.GetWaterfallStyle(30) is null, "vanilla and gaps untouched");
        Expect<InvalidOperationException>(() => loader.Register("late", new TestWaterfall(), _ => true),
            "registration frozen after Seal");
        loader.Activate();
        Check(loader.IsActive, "native texture installation gate");

        float r = 0.8f, g = 0.4f, b = 0.2f;
        Check(loader.TryColorMultiplier(28, ref r, ref g, ref b, 0.5f), "managed color hook");
        Check(Math.Abs(r - 0.4f) < 0.001f && Math.Abs(g - 0.2f) < 0.001f &&
              Math.Abs(b - 0.1f) < 0.001f, "ref float signature (no JS byte conversion)");
        Check(loader.ColorCalls == 1, "color metrics");
        Check(!loader.TryColorMultiplier(28, ref r, ref g, ref b, float.NaN), "reject NaN alpha");
        first.FailColor = true;
        Check(!loader.TryColorMultiplier(28, ref r, ref g, ref b, 0.5f), "fault isolated");
        Check(Math.Abs(r - 0.4f) < 0.001f && Math.Abs(g - 0.2f) < 0.001f,
            "color rolls back after a failed callback");
        Check(loader.RejectedCalls == 1, "diagnostic rejection counter");
        first.FailColor = false;

        Check(loader.TryAddLight(28, 10, 20, 10, sink), "native light request dispatch");
        Check(sink.Lights.Count == 4, "bounded sampling at 0,3,6,9");
        Check(sink.Lights[0] == new WaterfallLight(10, 20, 0.1f, 0.2f, 0.3f),
            "RGB and tile positions forwarded");
        Check(sink.Lights[3].TileY == 29, "last sampled tile");
        sink.Lights.Clear();
        Check(loader.TryAddLight(28, 10, 20, 500, sink), "long waterfall clamped to 24");
        Check(sink.Lights.Count == 9, "maximum nine samples, not 500 callbacks");
        Check(first.LightCalls == 13, "exact managed callback count");
        Check(loader.LightCalls == 2, "light dispatch metrics");
        Check(!loader.TryAddLight(27, 10, 20, 9, sink), "no vanilla interceptions");
        Check(!loader.TryAddLight(28, -1, 20, 9, sink), "negative tile rejected");
        Expect<InvalidOperationException>(() => WaterfallLighting.AddLight(0, 0, 1, 1, 1),
            "no native access outside the light callback");

        first.FailLight = true;
        Check(!loader.TryAddLight(28, 10, 20, 0, sink), "failed light hook isolated");
        Check(loader.RejectedCalls == 2, "light diagnostics");
        first.FailLight = false;

        loader.Unload();
        Check(!loader.IsActive && loader.TotalCount == 28, "unload deactivates and resets slots");
        Check(first.UnloadCount == 1 && second.UnloadCount == 1, "Unload called per style");
        Check(first.Slot == -1 && first.FullName == "", "clears session identity");
        Check(loader.ColorCalls == 0 && loader.LightCalls == 0 && loader.RejectedCalls == 0,
            "clears metrics between worlds");
        Check(loader.Register("testmod", first, _ => true) == 28, "re-register in a new session");

        var invalid = new InvalidTextureWaterfall();
        Expect<InvalidOperationException>(() => new WaterfallStyleLoader().Register(
            "testmod", invalid, _ => true), "path traversal blocked");

        var excessive = new ExcessiveLightWaterfall();
        var bounded = new WaterfallStyleLoader();
        bounded.Register("testmod", excessive, _ => true);
        bounded.Seal();
        bounded.Activate();
        Check(!bounded.TryAddLight(28, 0, 0, 0, new CapturedLights()), "light flood rejected");
        Check(bounded.RejectedCalls == 1, "flood rejection tracked");
    }

    private sealed class CapturedLights : IWaterfallLightSink
    {
        public List<WaterfallLight> Lights { get; } = new();
        public float R = 1, G = 1, B = 1;
        public void Enqueue(in WaterfallLight light) => Lights.Add(light);
    }

    private sealed class TestWaterfall : ModWaterfallStyle
    {
        public override string Texture => "Content/Biomes/TestWaterfall.png";
        public int LoadCount, StaticDefaultsCount, UnloadCount, LightCalls;
        public bool FailColor, FailLight;
        public override void Load() => LoadCount++;
        public override void SetStaticDefaults() => StaticDefaultsCount++;
        public override void Unload() => UnloadCount++;
        public override void ColorMultiplier(ref float r, ref float g, ref float b, float a)
        {
            r *= a; g *= a; b *= a;
            if (FailColor) throw new ArithmeticException("color");
        }
        public override void AddLight(int i, int j)
        {
            LightCalls++;
            if (FailLight) throw new InvalidOperationException("light");
            WaterfallLighting.AddLight(i, j, 0.1f, 0.2f, 0.3f);
        }
    }

    private sealed class DisabledWaterfall : ModWaterfallStyle
    {
        public override bool IsLoadingEnabled() => false;
    }

    private sealed class InvalidTextureWaterfall : ModWaterfallStyle
    {
        public override string Texture => "../outside.png";
    }

    private sealed class ExcessiveLightWaterfall : ModWaterfallStyle
    {
        public override string Texture => "Content/Biomes/Excessive.png";
        public override void AddLight(int i, int j)
        {
            for (int k = 0; k < 13; k++)
                WaterfallLighting.AddLight(i, j, 1, 1, 1);
        }
    }
}
