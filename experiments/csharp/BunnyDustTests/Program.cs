using BunnyLoader.Source;

static class DustTests
{
    private static int _passed;

    private static void Check(bool condition, string name)
    {
        if (!condition) throw new Exception("FAIL: " + name);
        _passed++;
    }

    private static void Throws<T>(Action action, string name) where T : Exception
    {
        try { action(); }
        catch (T) { _passed++; return; }
        throw new Exception("FAIL: expected " + typeof(T).Name + " at " + name);
    }

    private sealed class DefaultDust : BunnyDust
    {
        public override void SetStaticDefaults()
        {
            Texture = "Content/Gores/ExampleDroplet.png";
        }
    }

    private sealed class AnimatedDust : BunnyDust
    {
        public override void SetStaticDefaults()
        {
            Texture = "Content/Dusts/Animated.png";
            UpdateType = 6;
        }

        public override void OnSpawn(ref BunnyDustState state)
        {
            state.Scale = 1.2f;
        }

        public override bool Update(ref BunnyDustState state)
        {
            state.VelocityX += 0.5f;
            return false;
        }

        public override bool MidUpdate(ref BunnyDustState state)
        {
            return true;
        }

        public override bool GetAlpha(ref BunnyDustState state, ref uint color)
        {
            color = 0xff225577;
            return true;
        }

        public override bool PreDraw(ref BunnyDustState state)
        {
            return false;
        }
    }

    private sealed class FaultingDust : BunnyDust
    {
        public override void SetStaticDefaults() { }

        public override bool Update(ref BunnyDustState state)
        {
            state.X = float.NaN;
            return false;
        }

        public override bool PreDraw(ref BunnyDustState state) => throw new InvalidOperationException();
    }

    private sealed class ExhaustedDust : BunnyDust
    {
        public override void SetStaticDefaults() { }

        public override void OnSpawn(ref BunnyDustState state)
        {
            state.Scale = 0f;
            state.Kill();
        }
    }

    private sealed class InvalidActiveScaleDust : BunnyDust
    {
        public override void SetStaticDefaults() { }

        public override void OnSpawn(ref BunnyDustState state)
        {
            state.Scale = 0f;
        }
    }

    private sealed class InvalidTextureDust : BunnyDust
    {
        public override void SetStaticDefaults() => Texture = "../escaped.png";
    }

    private sealed class InvalidUpdateTypeDust : BunnyDust
    {
        public override void SetStaticDefaults() => UpdateType = 332;
    }

    private static BunnyDustState SpawnState(int type, int slot = 0, int generation = 1) => new()
    {
        Slot = slot,
        Type = type,
        Generation = generation,
        Active = 1,
        X = 10,
        Y = 12,
        VelocityX = 2,
        VelocityY = -1,
        Scale = 1,
        Alpha = 0,
        FrameWidth = 10,
        FrameHeight = 10,
        Color = 0xffffffff
    };

    private static void Run()
    {
        Check(System.Runtime.InteropServices.Marshal.SizeOf<BunnyDustState>() == 64, "managed/native dust layout");
        Check(System.Runtime.InteropServices.Marshal.OffsetOf<BunnyDustState>(nameof(BunnyDustState.Color)).ToInt32() == 60,
            "managed/native color offset");
        Throws<ArgumentException>(() => new BunnyDustRegistry("invalid/id"), "reject bad mod id");
        Throws<ArgumentException>(() => new BunnyDustRegistry("../escape"), "reject mod traversal");
        Throws<ArgumentException>(() => new BunnyDustRegistry(""), "reject empty mod id");
        var guidRegistry = new BunnyDustRegistry("57a98f20-7a35-4d19-8ce8-079e25bb36a1");
        Check(guidRegistry.Register("GuidDust", new DefaultDust()).Name == "GuidDust",
            "accept real CSharp GUID mod IDs with hyphens");

        var registry = new BunnyDustRegistry("dust_test");
        var defaultDust = registry.Register("Droplet", new DefaultDust());
        Check(defaultDust.Name == "Droplet" && defaultDust.UpdateType == -1, "metadata");
        Check(defaultDust.Hooks == BunnyDustHooks.None, "no unnecessary callbacks");
        Check(defaultDust.Texture == "Content/Gores/ExampleDroplet.png", "asset path");
        Check(registry.TryGetDefinition("Droplet", out var found) && found == defaultDust, "lookup");
        Check(!registry.TryGetDefinition("missing", out _), "missing name");

        Throws<InvalidOperationException>(() => registry.Register("Droplet", new DefaultDust()), "duplicate name");
        Throws<ArgumentException>(() => registry.Register("BadAsset", new InvalidTextureDust()), "asset traversal");
        Throws<ArgumentOutOfRangeException>(() => registry.Register("BadType", new InvalidUpdateTypeDust()), "vanilla update range");

        var animated = registry.Register("Animated", new AnimatedDust());
        Check(animated.Hooks == (BunnyDustHooks.OnSpawn | BunnyDustHooks.Update |
            BunnyDustHooks.MidUpdate | BunnyDustHooks.GetAlpha | BunnyDustHooks.PreDraw), "dynamic hooks");
        Check(animated.UpdateType == 6, "native update type");

        Check(!registry.Bind(331, "Droplet"), "reject vanilla dust type");
        Check(registry.Bind(332, "Droplet"), "bind first type");
        Check(registry.Bind(332, "Droplet"), "idempotent bind");
        Check(!registry.Bind(333, "Droplet"), "reject rebinding");
        Check(!registry.Bind(332, "Animated"), "reject conflicting type");
        Check(registry.Bind(333, "Animated"), "bind second type");
        Check(!registry.Bind(334, "Missing"), "reject unknown name");
        Check(registry.RegisteredCount == 2 && registry.BoundCount == 2, "counts");

        var plain = SpawnState(332, 5);
        Check(registry.OnSpawn(ref plain), "plain spawn");
        Check(registry.ActiveCount == 1, "active count");
        Check(registry.Update(ref plain, out bool plainVanilla) && plainVanilla, "plain native update");
        Check(registry.MidUpdate(ref plain, out bool plainSpeed) && !plainSpeed, "plain native friction");
        uint unchanged = 0xff010203;
        Check(!registry.GetAlpha(ref plain, ref unchanged) && unchanged == 0xff010203, "plain native alpha");
        Check(registry.PreDraw(ref plain, out bool plainDraw) && plainDraw, "plain native drawing");

        var sample = SpawnState(333, 6, 3);
        Check(registry.OnSpawn(ref sample) && Math.Abs(sample.Scale - 1.2f) < 0.001f, "spawn callback");
        Check(registry.Update(ref sample, out bool vanilla) && !vanilla && sample.VelocityX == 2.5f, "update callback");
        Check(registry.MidUpdate(ref sample, out bool speed) && speed, "midupdate callback");
        uint color = 0xff123456;
        Check(registry.GetAlpha(ref sample, ref color) && color == 0xff225577, "alpha callback");
        Check(registry.PreDraw(ref sample, out bool draw) && !draw, "predraw callback");
        Check(registry.ActiveCount == 2, "two slots");
        registry.Release(6, 333, 2);
        Check(registry.ActiveCount == 2, "ignore stale generation release");
        registry.Release(6, 333, 3);
        Check(registry.ActiveCount == 1, "release matching instance");
        Check(!registry.Update(ref sample, out _), "reject stale callback");

        registry.Register("Faulty", new FaultingDust());
        Check(registry.Bind(334, "Faulty"), "bind fault test");
        var fault = SpawnState(334, 7);
        Check(registry.OnSpawn(ref fault), "fault test spawn");
        float priorX = fault.X;
        Check(!registry.Update(ref fault, out bool fallback) && fallback, "safe vanilla fallback");
        Check(fault.X == priorX && registry.FaultCount == 1, "rollback on invalid state");
        Check(registry.PreDraw(ref fault, out bool visible) && visible, "quarantined draw fallback");
        Check(registry.Update(ref fault, out bool quarantinedVanilla) && quarantinedVanilla,
            "do not retry failed callbacks every frame");
        Check(registry.FaultCount == 1 && registry.LastError.Contains("Update"), "per-instance isolation");

        var next = SpawnState(332, 5, 2);
        Check(registry.OnSpawn(ref next), "slot reuse");
        registry.Release(5, 332, 1);
        Check(registry.ActiveCount == 2, "old generation cannot release reused slot");
        registry.Release(5, 332, 2);
        Check(registry.ActiveCount == 1, "new generation release");

        var zero = new BunnyDustRegistry("zero_scale");
        zero.Register("Exhausted", new ExhaustedDust());
        Check(zero.Bind(332, "Exhausted"), "bind dying Dust");
        var dying = SpawnState(332, 100);
        Check(zero.OnSpawn(ref dying) && dying.Active == 0 && dying.Scale == 0f,
            "allow zero scale when killed by OnSpawn");
        Check(zero.ActiveCount == 0, "release instance killed during spawn");
        zero.Register("InvalidActive", new InvalidActiveScaleDust());
        Check(zero.Bind(333, "InvalidActive"), "bind invalid live scale Dust");
        dying = SpawnState(333, 101);
        Check(!zero.OnSpawn(ref dying) && dying.Active == 1 &&
            dying.Scale == 1f && zero.FaultCount == 1,
            "reject zero scale while active and roll back state");

        var sharedTypes = new BunnyDustRegistry("shared_ids");
        sharedTypes.Register("LaterMod", new DefaultDust());
        Check(sharedTypes.Bind(5000, "LaterMod"), "allow native IDs beyond local type count");
        Check(!sharedTypes.Bind(32768, "LaterMod"), "bound native ID allocation");
        sharedTypes.Unload();

        registry.ResetWorld();
        Check(registry.ActiveCount == 0 && registry.BoundCount == 3, "world reset preserves registered types");
        registry.Unload();
        Check(registry.ActiveCount == 0 && registry.BoundCount == 0 &&
            registry.RegisteredCount == 0 && registry.FaultCount == 0, "unload clears state");
        Console.WriteLine("DustLoader managed contract: " + _passed + " checks passed.");
    }

    public static int Main()
    {
        Run();
        return 0;
    }
}
