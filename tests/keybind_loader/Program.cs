using BunnyLoader.Source;

static class KeybindContract
{
    private static int checks;

    private static void Check(bool ok, string description)
    {
        if (!ok) throw new InvalidOperationException("Keybind: " + description);
        checks++;
    }

    private static void Rejected(Action test, string description)
    {
        bool rejected = false;
        try { test(); } catch (ArgumentException) { rejected = true; }
        Check(rejected, description);
    }

    public static void Main()
    {
        Rejected(() => BunnyTouchButtonLoader.RegisterButton("../escape", "Dash"), "owner traversal");
        Rejected(() => BunnyTouchButtonLoader.RegisterButton("example", "Dash/Bad"), "name with slash");
        Rejected(() => BunnyTouchButtonLoader.RegisterButton("example", "Dash",
            new BunnyTouchButtonOptions { Icon = "../../escape.png" }), "icon traversal");
        BunnyTouchButton dash = BunnyTouchButtonLoader.RegisterKeybind("example", "Dash", "Q");
        BunnyTouchButton dashAgain = BunnyTouchButtonLoader.RegisterKeybind("example", "Dash", "R");
        BunnyTouchButton second = BunnyTouchButtonLoader.RegisterButton("second", "Dash",
            new BunnyTouchButtonOptions { Label = "Corrida rápida" });
        Check(ReferenceEquals(dash, dashAgain), "idempotent registration");
        Check(dash.FullName == "example/Dash", "stable full name");
        Check(second.FullName == "second/Dash", "different mod identity");
        Check(second.Id != dash.Id && BunnyTouchButtonLoader.Count == 2, "unique session slots");
        Check(BunnyTouchButtonLoader.TryGet(dash.FullName, out BunnyTouchButton? found) &&
              ReferenceEquals(found, dash), "lookup");
        Check(dash.GetAssignedKeys().Length == 0, "unbound key");

        BunnyTouchButtonLoader.ApplyHostFrame(1, true, false, false, false,
            new[] { dash.Id }, Array.Empty<int>(), new[] { dash.Id });
        Check(dash.Current && dash.JustPressed && !dash.JustReleased && !dash.Old, "first press");
        Check(dash.RetroCurrent && dash.IsBound && dash.GetAssignedKeys().Length == 1, "bound key and retro");
        Check(!second.Current && !second.IsBound, "isolated other mod");

        Check(BunnyTouchButtonLoader.ApplyHostFrame(1, true, false, false, false,
            Array.Empty<int>(), Array.Empty<int>(), Array.Empty<int>()) == 0, "dedup tick");
        Check(dash.Current, "duplicate tick cannot erase press");

        BunnyTouchButtonLoader.ApplyHostFrame(2, true, true, false, false,
            new[] { dash.Id }, Array.Empty<int>(), new[] { dash.Id });
        Check(dash.Current && dash.Old && !dash.JustPressed && !dash.RetroCurrent, "chat blocks retro");
        BunnyTouchButtonLoader.ApplyHostFrame(3, true, false, false, false,
            Array.Empty<int>(), Array.Empty<int>(), new[] { dash.Id });
        Check(dash.JustReleased && !dash.Current && dash.Old, "release");

        BunnyTouchButtonLoader.ApplyHostFrame(4, true, false, false, false,
            Array.Empty<int>(), new[] { dash.Id }, new[] { dash.Id });
        Check(dash.JustPressed && dash.Current, "sub-frame tap");
        BunnyTouchButtonLoader.ApplyHostFrame(5, true, false, false, false,
            Array.Empty<int>(), Array.Empty<int>(), new[] { dash.Id });
        Check(dash.JustReleased, "sub-frame tap released on next tick");

        BunnyTouchButtonLoader.ApplyHostFrame(6, true, false, false, false,
            new[] { second.Id }, Array.Empty<int>(), new[] { second.Id });
        Check(second.JustPressed && second.Current && !dash.Current, "per-button state");
        BunnyTouchButtonLoader.ApplyHostFrame(7, false, false, false, false,
            new[] { second.Id }, Array.Empty<int>(), new[] { second.Id });
        Check(second.JustReleased && !second.Current, "menu drops held state");
        BunnyTouchButtonLoader.ApplyHostFrame(8, true, false, true, false,
            new[] { second.Id }, Array.Empty<int>(), new[] { second.Id });
        Check(second.Current && !second.RetroCurrent, "NPC dialogue blocks retro");
        BunnyTouchButtonLoader.ApplyHostFrame(9, true, false, false, true,
            new[] { second.Id }, Array.Empty<int>(), new[] { second.Id });
        Check(second.Current && !second.RetroCurrent, "sign blocks retro");

        second.SetVisible(false);
        Check(!second.Visible && second.Enabled, "visibility policy update");
        BunnyTouchButtonLoader.ApplyHostFrame(10, true, false, false, false,
            new[] { second.Id }, new[] { second.Id }, new[] { second.Id });
        Check(!second.Current && !second.JustPressed && second.IsBound,
            "invisible button cannot be activated");
        second.SetVisible(true);
        second.SetEnabled(false);
        BunnyTouchButtonLoader.ApplyHostFrame(11, true, false, false, false,
            new[] { second.Id }, new[] { second.Id }, new[] { second.Id });
        Check(!second.Current && !second.JustPressed,
            "disabled button cannot be activated");
        second.SetEnabled(true);
        BunnyTouchButtonLoader.ApplyHostFrame(12, true, false, false, false,
            new[] { second.Id }, Array.Empty<int>(), new[] { second.Id });
        Check(second.Current && second.JustPressed && second.Visible && second.Enabled,
            "re-enabled button resumes without stale latch");

        BunnyTouchButtonLoader.ReleaseAll();
        Check(!second.Current && !second.JustPressed && !second.IsBound, "world reset");
        BunnyTouchButtonLoader.ApplyHostFrame(1, true, false, false, false,
            new[] { second.Id }, Array.Empty<int>(), new[] { second.Id });
        Check(second.JustPressed, "new world with fresh tick epoch");

        BunnyTouchButtonLoader.UnregisterMod("example");
        Check(!BunnyTouchButtonLoader.TryGet("example/Dash", out _), "unloaded mod removed");
        Check(BunnyTouchButtonLoader.Count == 1, "other mod remains");
        Check(!dash.Current, "stale ID cannot retain input");
        BunnyTouchButtonLoader.ApplyHostFrame(2, true, false, false, false,
            new[] { dash.Id, 127, -1 }, Array.Empty<int>(), Array.Empty<int>());
        Check(!second.Current && !dash.Current, "invalid/stale IDs discarded");
        LayoutStoreTests.Run();
        Console.WriteLine("KEYBIND CONTRACT PASS (" + checks + " checks)");
    }
}
