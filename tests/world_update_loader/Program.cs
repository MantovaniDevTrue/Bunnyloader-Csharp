using System;
using System.Reflection;
using BunnyLoader.Source;
using ExampleModCSharp;

static class Program
{
    private static int assertions;

    private static void Check(bool condition, string description)
    {
        if (!condition) throw new InvalidOperationException(description);
        assertions++;
    }

    private sealed class EmptyWorldSystem : BunnyWorldUpdateLoader { }

    private sealed class DisabledWorldSystem : BunnyWorldUpdateLoader
    {
        protected override bool EnableUpdates => false;
        protected override void PreUpdateEntities() { }
        protected override void PostUpdateEverything() { }
    }

    private static void Main()
    {
        Check(typeof(BunnyHookModule).IsAssignableFrom(typeof(BunnyWorldUpdateLoader)),
            "Sistema não pertence ao dispatcher C# existente");
        Check(typeof(BunnyWorldUpdateLoader).IsAbstract,
            "A classe base não deve ser instanciada diretamente");

        string[] events =
        {
            "PreUpdateEntities", "PostUpdateEverything", "PreUpdateProjectiles",
            "PreUpdateDusts", "PostUpdateDusts", "PreUpdateInvasions",
            "PostUpdateInvasions", "PostUpdateInput"
        };
        foreach (string name in events)
        {
            MethodInfo? method = typeof(BunnyWorldUpdateLoader).GetMethod(
                name, BindingFlags.Instance | BindingFlags.NonPublic);
            Check(method is not null && method.IsVirtual && method.IsFamily && !method.IsAbstract,
                "Callback não implementado: " + name);
        }

        // Caminhos sem hooks são testáveis no desktop e não tocam a biblioteca
        // ARM64. O teste funcional dos cinco métodos nativos exige Android.
        var disabled = new DisabledWorldSystem();
        var noOverrides = new EmptyWorldSystem();
        var exampleA = new ExampleWorldUpdateCounter();
        var exampleB = new ExampleDustAndInvasionSystem();
        using var scope = new BunnyHookScope();
        disabled.Load(scope);
        noOverrides.Load(scope);
        exampleA.Load(scope);
        exampleB.Load(scope);
        foreach (BunnyWorldUpdateLoader module in new BunnyWorldUpdateLoader[]
                 { disabled, noOverrides, exampleA, exampleB })
        {
            Check(!module.IsRegistered, "Sistema sem inscrição ligou hook nativo");
            Check(module.CallbackCount == 0 && module.ErrorCount == 0,
                "Contadores deveriam começar zerados");
            module.Unload();
        }
        Check(typeof(ExampleWorldUpdateCounter).GetMethod("DiagnosticStatus") is not null,
            "Exemplo de contador do mundo ausente");
        Check(typeof(ExampleDustAndInvasionSystem).GetMethod("DiagnosticStatus") is not null,
            "Exemplo de contador de poeira ausente");

        Console.WriteLine("SystemUpdateLoader C#: " + assertions +
            " verificações de API/compilação passaram (sem teste Android)");
    }
}
