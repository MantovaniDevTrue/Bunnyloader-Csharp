using BunnyLoader.Source;
using System;
using System.IO;

internal static class LayoutStoreTests
{
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException("Touch layouts: " + message);
    }

    internal static void Run()
    {
        var doc = new BunnyTouchLayoutStore();
        var layout = new[]
        {
            new BunnyTouchLayoutSlot
            {
                SlotId = "A", Index = 0, ActionUnbound = true,
                ActionFullName = "example/Dash",
                GroupActions = new string?[] { "second/Blink", null },
                GroupUnbound = new[] { true, true }
            },
            new BunnyTouchLayoutSlot
            {
                SlotId = "B", Index = 1, ActionUnbound = false,
                GroupActions = Array.Empty<string?>(),
                GroupUnbound = Array.Empty<bool>()
            }
        };
        bool Loaded(string action) => action is "example/Dash" or "second/Blink";
        doc.Capture("main", layout, Loaded);
        Check(doc.ProfileCount == 1 && doc.HasProfile("main"), "profile captured");
        string json = doc.Serialize();
        Check(json.Contains("\"profiles\"") && json.Contains("\"slot\":\"A\"") &&
              json.Contains("\"action\":\"example/Dash\"") &&
              !json.Contains("\"slot\":\"B\""), "canonical JS JSON schema and sparse rows");
        doc = BunnyTouchLayoutStore.Parse(json);
        Check(doc.ProfileCount == 1, "round trip");

        // Ao voltar, restaura ações de mod somente onde o perfil vanilla está vazio.
        var restored = new[]
        {
            new BunnyTouchLayoutSlot
            {
                SlotId = "A", Index = 0, ActionUnbound = true,
                GroupActions = new string?[2],
                GroupUnbound = new[] { true, true }
            }
        };
        doc.Restore("main", restored, Loaded);
        Check(restored[0].ActionFullName == "example/Dash", "mod action restored");
        Check(restored[0].GroupActions[0] == "second/Blink", "mod group restored");
        Check(restored[0].GroupActions[1] is null, "unbound group stays unbound");

        // Ação vanilla nunca pode ser apagada para encaixar uma ação de mod.
        restored[0].ActionFullName = null;
        restored[0].ActionUnbound = false;
        doc.Restore("main", restored, Loaded);
        Check(restored[0].ActionFullName is null, "vanilla action preserved");

        // O botão de um mod removido continua salvo enquanto o slot vanilla
        // estiver vazio; reaparece ao reativar o mod.
        var unavailable = new[]
        {
            new BunnyTouchLayoutSlot
            {
                SlotId = "A", Index = 0, ActionUnbound = true,
                GroupActions = new string?[2],
                GroupUnbound = new[] { true, true }
            }
        };
        doc.Capture("main", unavailable, action => action == "second/Blink");
        var missing = new[]
        {
            new BunnyTouchLayoutSlot
            {
                SlotId = "A", Index = 0, ActionUnbound = true,
                GroupActions = new string?[2],
                GroupUnbound = new[] { true, true }
            }
        };
        doc.Restore("main", missing, action => action == "second/Blink");
        Check(missing[0].ActionFullName is null, "missing mod not bound");
        doc.Restore("main", missing, Loaded);
        Check(missing[0].ActionFullName == "example/Dash", "missing mod kept by identity");

        // Outros perfis persistem quando o jogador salva apenas o perfil ativo.
        doc.Capture("alternate", layout, Loaded);
        doc.Capture("main", Array.Empty<BunnyTouchLayoutSlot>(), Loaded);
        Check(!doc.HasProfile("main") && doc.HasProfile("alternate"), "other profiles retained");

        var reordered = new[]
        {
            new BunnyTouchLayoutSlot
            {
                SlotId = "A", Index = 5, ActionUnbound = true,
                GroupActions = new string?[2],
                GroupUnbound = new[] { true, true }
            }
        };
        doc.Restore("alternate", reordered, Loaded);
        Check(reordered[0].ActionFullName == "example/Dash", "SlotId wins over index");

        string directory = Path.Combine(Path.GetTempPath(), "bunny-touch-layout-" + Guid.NewGuid().ToString("N"));
        try
        {
            doc.Save(directory);
            doc = BunnyTouchLayoutStore.Load(directory);
            Check(doc.HasProfile("alternate"), "atomic save and reload");
            File.WriteAllText(Path.Combine(directory, "BunnyLoader.touchbuttons.csharp.json"),
                              "{\"version\":2,\"profiles\":{}}");
            bool corrupted = false;
            try { BunnyTouchLayoutStore.Load(directory); }
            catch (InvalidDataException) { corrupted = true; }
            Check(corrupted, "future schema rejected instead of overwritten");
        }
        finally
        {
            if (Directory.Exists(directory)) Directory.Delete(directory, true);
        }

        void Reject(string data, string label)
        {
            bool failed = false;
            try { BunnyTouchLayoutStore.Parse(data); }
            catch (InvalidDataException) { failed = true; }
            Check(failed, label);
        }
        Reject("{\"version\":1,\"profiles\":{\"x\":[{\"slot\":\"S\",\"index\":0,\"action\":\"../../escape\"}]}}",
               "malformed action rejected");
        Reject("{\"version\":1,\"profiles\":{\"x\":[{\"slot\":\"S\",\"index\":0},{\"slot\":\"S\",\"index\":1}]}}",
               "duplicate slot rejected");
        Reject("{\"version\":1,\"profiles\":{\"x\":[{\"slot\":\"S\",\"index\":999}]}}",
               "out of range index rejected");
        Reject("{\"version\":1,\"profiles\":[]}", "profiles must be object");
        Console.WriteLine("TOUCH LAYOUT STORE PASS");
    }
}
