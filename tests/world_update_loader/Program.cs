using System;
using System.Collections.Generic;
using System.Reflection;
using BunnyLoader.Source;
using ExampleModCSharp;

static class Program
{
    private static int assertions;
    private static readonly List<string> Sequence = new();
    private static readonly List<(BunnyHookScope Scope, string Method, bool Static, BunnyMethodHook Hook)> Hooks = new();
    private static int installs;

    private sealed class EmptyWorldSystem : BunnyWorldUpdateLoader { }

    private sealed class DisabledWorldSystem : BunnyWorldUpdateLoader
    {
        protected override bool EnableUpdates => false;
        protected override void PreUpdateEntities() => throw new Exception("Desligado");
    }

    private sealed class A : BunnyWorldUpdateLoader
    {
        internal bool ThrowOnBefore;
        protected override void PreUpdateEntities()
        {
            Sequence.Add("A-pre");
            if (ThrowOnBefore) throw new ArgumentException("teste isolado");
        }
        protected override void PostUpdateEverything() => Sequence.Add("A-post");
        protected override void PreUpdateProjectiles() => Sequence.Add("A-proj");
        protected override void PreUpdateDusts() => Sequence.Add("A-dust-pre");
        protected override void PostUpdateDusts() => Sequence.Add("A-dust-post");
        protected override void PreUpdateInvasions() => Sequence.Add("A-invasion-pre");
        protected override void PostUpdateInvasions() => Sequence.Add("A-invasion-post");
        protected override void PostUpdateInput() => Sequence.Add("A-input");
    }

    private sealed class B : BunnyWorldUpdateLoader
    {
        protected override void PreUpdateEntities() => Sequence.Add("B-pre");
        protected override void PostUpdateEverything() => Sequence.Add("B-post");
        protected override void PostUpdateInput() => Sequence.Add("B-input");
    }

    private sealed class C : BunnyWorldUpdateLoader
    {
        protected override void PreUpdateEntities() => Sequence.Add("C-pre");
        protected override void PostUpdateEverything() => Sequence.Add("C-post");
    }

    private static void Check(bool condition, string description)
    {
        if (!condition) throw new InvalidOperationException(description);
        assertions++;
    }

    private static void Order(string expected)
    {
        Check(string.Join(",", Sequence) == expected,
            "Ordem incorreta: " + string.Join(",", Sequence) + " versus " + expected);
        Sequence.Clear();
    }

    private static BunnyMethodHook FakeInstall(
        BunnyHookScope scope, string type, string method, BunnyVoidHook callback, bool isStatic)
    {
        Check(type == "Terraria.Main" || type == "Terraria.Dust", "Tipo de host desconhecido");
        bool expectedStatic = method == "UpdateDust" || method == "UpdateInvasion";
        Check(isStatic == expectedStatic, "ABI estático/instância inválido: " + method);
        var handle = new BunnyMethodHook(callback);
        Hooks.Add((scope, method, isStatic, handle));
        installs++;
        return handle;
    }

    private static void Fire(string method, string stage)
    {
        BunnyWorldUpdateRegistry.TestFire(method, () => Sequence.Add(stage));
    }

    private static void Main()
    {
        Check(typeof(BunnyHookModule).IsAssignableFrom(typeof(BunnyWorldUpdateLoader)),
            "Loader não pertence ao dispatcher C#");
        Check(typeof(BunnyWorldUpdateLoader).IsAbstract, "Classe-base não é abstrata");
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
            Check(method is not null && method.IsVirtual && method.IsFamily,
                "Callback ausente: " + name);
        }

        using var scopeA = new BunnyHookScope();
        using var scopeB = new BunnyHookScope();
        BunnyWorldUpdateRegistry.TestHookFactory = FakeInstall;

        var empty = new EmptyWorldSystem();
        var disabled = new DisabledWorldSystem();
        var exampleA = new ExampleWorldUpdateCounter();
        var exampleB = new ExampleDustAndInvasionSystem();
        empty.Load(scopeA);
        disabled.Load(scopeA);
        exampleA.Load(scopeA);
        exampleB.Load(scopeA);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 0, "Sistemas inativos instalaram hooks");
        Check(!exampleA.IsRegistered && !exampleB.IsRegistered && !disabled.IsRegistered,
            "O opt-in falhou");

        var a = new A();
        var b = new B();
        var c = new C();
        a.Load(scopeA);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 5, "Faltam hooks dos cinco estágios");
        Check(installs == 5, "Quantidade inicial de hooks");
        b.Load(scopeA);
        c.Load(scopeB);
        Check(installs == 5, "Dois mods instalaram hooks duplicados");
        Check(a.IsRegistered && b.IsRegistered && c.IsRegistered,
            "Inscrições ativas não registradas");

        Fire("DoUpdateInWorld", "vanilla");
        Order("A-pre,B-pre,C-pre,vanilla,A-post,B-post,C-post");
        Fire("PreUpdateAllProjectiles", "vanilla");
        Order("A-proj,vanilla");
        Fire("UpdateDust", "vanilla");
        Order("A-dust-pre,vanilla,A-dust-post");
        Fire("UpdateInvasion", "vanilla");
        Order("A-invasion-pre,vanilla,A-invasion-post");
        Fire("DoUpdate_HandleInput", "vanilla");
        Order("vanilla,A-input,B-input");

        a.ThrowOnBefore = true;
        Fire("DoUpdateInWorld", "vanilla");
        Order("A-pre,B-pre,C-pre,vanilla,A-post,B-post,C-post");
        Check(a.ErrorCount == 1 && a.LastError.Contains("PreUpdateEntities"),
            "Falha de mod não foi contida");
        a.ThrowOnBefore = false;

        bool originalFailed = false;
        try
        {
            BunnyWorldUpdateRegistry.TestFire("DoUpdateInWorld", () =>
            {
                Sequence.Add("original-falhou");
                throw new ApplicationException("erro vanilla simulado");
            });
        }
        catch (ApplicationException) { originalFailed = true; }
        Check(originalFailed, "Exceção vanilla foi silenciosamente engolida");
        Order("A-pre,B-pre,C-pre,original-falhou,A-post,B-post,C-post");

        // O dispatcher destrói o escopo do mod antes de chamar Unload()
        // nos módulos. O registry recebe esse aviso uma única vez.
        BunnyWorldUpdateRegistry.DetachScope(scopeA);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 1,
            "A saída do mod A manteve hooks sem listeners");
        Check(installs == 6, "O hook não migrou para o escopo do mod C");
        a.Unload();
        b.Unload();
        empty.Unload();
        disabled.Unload();
        exampleA.Unload();
        exampleB.Unload();
        scopeA.Dispose();
        Fire("DoUpdateInWorld", "vanilla");
        Order("C-pre,vanilla,C-post");
        Check(c.IsRegistered, "Segundo mod perdeu a inscrição ao sair o primeiro");

        BunnyWorldUpdateRegistry.DetachScope(scopeB);
        c.Unload();
        scopeB.Dispose();
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 0, "Hooks vivos após unload");
        Fire("DoUpdateInWorld", "vanilla");
        Order("vanilla");
        Check(Hooks.TrueForAll(h => h.Hook.IsDisposed),
            "Handle não foi liberado no unload");
        BunnyWorldUpdateRegistry.TestHookFactory = null;

        Console.WriteLine("SystemUpdateLoader C#: " + assertions +
            " verificações aprovadas; cinco alvos e três módulos simulados, sem APK.");
    }
}
