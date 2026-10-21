using System;
using System.Collections.Generic;
using System.Reflection;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using System.IO;
using System.Linq;
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
        protected override void PreUpdatePlayers() => Sequence.Add("A-players-pre");
        protected override void PostUpdatePlayers() => Sequence.Add("A-players-post");
        protected override void PreUpdateNPCs() => Sequence.Add("A-npcs-pre");
        protected override void PostUpdateNPCs() => Sequence.Add("A-npcs-post");
        protected override void PreUpdateGores() => Sequence.Add("A-gores-pre");
        protected override void PostUpdateGores() => Sequence.Add("A-gores-post");
        protected override void PostUpdateProjectiles() => Sequence.Add("A-projectiles-post");
        protected override void PreUpdateItems() => Sequence.Add("A-items-pre");
        protected override void PostUpdateItems() => Sequence.Add("A-items-post");
        protected override void UpdateUI(nint gameTime) => Sequence.Add("A-ui");
        protected override void ModifyTimeRate(ref BunnyTimeRateValues rates)
        {
            Sequence.Add("A-time");
            rates.TimeRate = 0.5;
            rates.TileUpdateRate = 1.25;
            rates.EventUpdateRate = 0.75;
        }
    }

    private sealed class B : BunnyWorldUpdateLoader
    {
        internal bool ThrowOnTime;
        protected override void ModifyTimeRate(ref BunnyTimeRateValues rates)
        {
            if (ThrowOnTime)
            {
                rates.TimeRate = double.NaN;
                throw new InvalidOperationException("Falha isolada em ModifyTimeRate");
            }
            rates.TimeRate += 0.25;
            rates.TileUpdateRate += 0.25;
            rates.EventUpdateRate += 0.25;
            Sequence.Add("B-time");
        }
        protected override void PreUpdateEntities() => Sequence.Add("B-pre");
        protected override void PostUpdateEverything() => Sequence.Add("B-post");
        protected override void PostUpdateInput() => Sequence.Add("B-input");
    }

    private sealed class OnlyInput : BunnyWorldUpdateLoader
    {
        protected override void PostUpdateInput() => Sequence.Add("input");
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
        Check(type == "Terraria.Main" || type == "Terraria.Dust" || type == "Bunny.SystemStage", "Tipo de host desconhecido");
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

    private static void VerifyStandaloneModule()
    {
        string folder = Path.Combine(Path.GetTempPath(), "bunny-standalone-world-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(folder,"content"));
        string framework = Path.Combine(folder,"framework"); Directory.CreateDirectory(framework);
        try {
            string[] references = {"System.Private.CoreLib.dll","System.Runtime.dll","System.Console.dll","System.Collections.dll",
                "System.Linq.dll","System.Threading.dll","System.Runtime.InteropServices.dll","System.Memory.dll","System.Reflection.dll",
                "System.Reflection.Extensions.dll","System.Runtime.Extensions.dll","netstandard.dll"};
            var trusted = ((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator)
                .GroupBy(Path.GetFileName,StringComparer.OrdinalIgnoreCase).ToDictionary(g=>g.Key!,g=>g.First(),StringComparer.OrdinalIgnoreCase);
            foreach(string name in references) File.Copy(trusted[name],Path.Combine(framework,name));
            File.Copy(typeof(BunnyItem).Assembly.Location,Path.Combine(framework,"BunnySourceSdk.dll"));
            File.WriteAllText(Path.Combine(folder,"content","World.cs"), """"
                using BunnyLoader.Source;
                public abstract class SharedWorld : BunnyWorldUpdateLoader {
                    private long ticks;
                    protected override void PostUpdateEverything() {
                        char open='{', close='}';
                        string verbatim=@"""} { """, interpolated=$"{new string('}', 1)}";
                        string raw=""" } " unbalanced { """;
                        int @class=3;
                        int delta=@class switch { > 2 when open=='{' && close=='}' => 1, _ => 0 };
                        System.Action callback=()=>System.Threading.Interlocked.Add(ref ticks,delta);
                        if(verbatim.Length>0 && interpolated=="}" && raw.Length>0) callback();
                    }
                    public override string DiagnosticStatus() => base.DiagnosticStatus() + " standaloneTicks=" + ticks;
                }
                public sealed class IndependentWorld : SharedWorld { }
                """");
            string output=Path.Combine(folder,"Independent.dll"), diagnostic=Path.Combine(folder,"compile.txt");
            Check(Compiler.Compile(folder,output,framework,diagnostic)==1,"Pure module failed real Roslyn compilation: "+File.ReadAllText(diagnostic));
            Check(CompiledAssemblyProbe.Verify(output,diagnostic,folder)==1 && CompiledAssemblyProbe.LastVerifiedHookModules==1,
                "Pure inherited module did not contribute to verified module inventory");
            Check(File.ReadAllText(Path.Combine(folder,"source-runtime-values.txt")).Contains("HOOK_MODULES\t1"),"Pure module inventory was not serialized");
            BunnyWorldUpdateRegistry.TestHookFactory=FakeInstall;
            Check(BunnyUniversalHookDispatcher.Register("independent-world-uid",output)==1,"Automatic module dispatcher failed: "+BunnyUniversalHookDispatcher.LastError);
            Check(BunnyWorldUpdateRegistry.ActiveHookCount==1,"Standalone module did not install its real override");
            Fire("EntitiesEnd","vanilla"); Sequence.Clear();
            Check(BunnyUniversalHookDispatcher.GetStatusSnapshot("independent-world-uid").Contains("standaloneTicks=1"),
                "Automatically discovered inherited module did not execute");
            Check(BunnyUniversalHookDispatcher.Unload("independent-world-uid") && BunnyWorldUpdateRegistry.ActiveHookCount==0,
                "Pure module did not unload its native stage subscription");
            BunnyWorldUpdateRegistry.TestHookFactory=null;
            Console.WriteLine("STANDALONE_INHERITED_WORLD_MODULE_ROSLYN_VERIFY_AUTO_DISCOVERY_DISPATCH_UNLOAD=PASS");
        } finally { Directory.Delete(folder,true); }
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
            "PostUpdateInvasions", "PostUpdateInput",
            "PreUpdatePlayers", "PostUpdatePlayers", "PreUpdateNPCs",
            "PostUpdateNPCs", "PreUpdateGores", "PostUpdateGores",
            "PostUpdateProjectiles", "PreUpdateItems", "PostUpdateItems", "UpdateUI", "ModifyTimeRate"
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
        exampleB.Load(scopeA);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 0, "Sistemas inativos instalaram hooks");
        Check(!exampleB.IsRegistered && !disabled.IsRegistered,
            "O opt-in falhou");

        using (var sampleScope = new BunnyHookScope()) {
            exampleA.Load(sampleScope);
            Check(exampleA.IsRegistered && BunnyWorldUpdateRegistry.ActiveHookCount == 2, "Example ativo nao registrou seus dois hooks");
            Fire("EntitiesEnd", "vanilla");
            Fire("PreUpdateAllProjectiles", "vanilla");
            Check(exampleA.CallbackCount == 2 && exampleA.DiagnosticStatus().Contains("mundo=1") &&
                exampleA.DiagnosticStatus().Contains("projeteis=1"), "Example real nao executou ambos os contadores");
            Sequence.Clear();
            BunnyWorldUpdateRegistry.DetachScope(sampleScope);
        }
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 0, "Example nao descarregou hooks");
        Hooks.Clear(); installs = 0;
        var a = new A();
        var b = new B();
        var c = new C();
        a.Load(scopeA);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 15, "Faltam hooks dos quinze estágios");
        Check(installs == 15, "Quantidade inicial de hooks");
        b.Load(scopeA);
        c.Load(scopeB);
        Check(installs == 15, "Dois mods instalaram hooks duplicados");
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
        Fire("PlayersBegin", "vanilla");
        Order("A-players-pre,vanilla");
        Fire("PlayersEnd", "vanilla");
        Order("A-players-post,vanilla");
        Fire("NPCsBegin", "vanilla");
        Order("A-npcs-pre,vanilla");
        Fire("NPCsEnd", "vanilla");
        Order("A-npcs-post,A-gores-pre,vanilla");
        Fire("GoresEnd", "vanilla");
        Order("A-gores-post,vanilla");
        Fire("ItemsBegin", "vanilla");
        Order("A-projectiles-post,A-items-pre,vanilla");
        Fire("ItemsEnd", "vanilla");
        Order("A-items-post,vanilla");
        Fire("UIEnd", "vanilla");
        Order("A-ui,vanilla");
        BunnyTimeRateValues applied = default;
        long appliedSession = 0;
        BunnyWorldUpdateRegistry.TestTimeRead = () =>
            (42, new BunnyTimeRateValues(1, 1, 1));
        BunnyWorldUpdateRegistry.TestTimeWrite = (session, rates) =>
        {
            appliedSession = session;
            applied = rates;
            return true;
        };
        Fire("TimeRate", "vanilla");
        Order("A-time,B-time,vanilla");
        Check(appliedSession == 42 && applied.TimeRate == 0.75 &&
              applied.TileUpdateRate == 1.5 && applied.EventUpdateRate == 1.0,
              "Valores de relógio não preservados entre nativo e CSharp");
        b.ThrowOnTime = true;
        long beforeFaults = b.ErrorCount;
        Fire("TimeRate", "vanilla");
        Order("A-time,vanilla");
        Check(b.ErrorCount == beforeFaults + 1 &&
              applied.TimeRate == 0.5 && applied.TileUpdateRate == 1.25 &&
              applied.EventUpdateRate == 0.75,
              "Exceção em ModifyTimeRate alterou o relógio de outro mod");
        b.ThrowOnTime = false;
        BunnyWorldUpdateRegistry.TestTimeRead = null;
        BunnyWorldUpdateRegistry.TestTimeWrite = null;

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
        Order("A-pre,B-pre,C-pre,original-falhou");

        // O dispatcher destrói o escopo do mod antes de chamar Unload()
        // nos módulos. O registry recebe esse aviso uma única vez.
        BunnyWorldUpdateRegistry.DetachScope(scopeA);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 2,
            "A saída do mod A manteve hooks sem listeners");
        Check(installs == 17, "Os hooks não migraram para o escopo do mod C");
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
        // Se o host impedir reinstalação durante o unload, não podemos
        // interromper a desmontagem do mod nem deixar handles órfãos.
        using var scopeD = new BunnyHookScope();
        using var scopeE = new BunnyHookScope();
        var d = new OnlyInput();
        var e = new OnlyInput();
        d.Load(scopeD);
        e.Load(scopeE);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 1,
            "Hook inicial do teste de falha ausente");
        bool failRebind = true;
        BunnyWorldUpdateRegistry.TestHookFactory = (scope, type, method, callback, isStatic) =>
        {
            if (failRebind)
            {
                failRebind = false;
                throw new NotSupportedException("Rebind recusado em unload");
            }
            return FakeInstall(scope, type, method, callback, isStatic);
        };
        long previousFaults = BunnyWorldUpdateRegistry.InstallFaultCount;
        BunnyWorldUpdateRegistry.DetachScope(scopeD);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 0,
            "Hook antigo continuou ativo depois de rebind recusado");
        Check(BunnyWorldUpdateRegistry.InstallFaultCount == previousFaults + 1 &&
              BunnyWorldUpdateRegistry.LastInstallError.Contains("Rebind recusado"),
            "Falha de rebind não foi registrada");
        d.Unload();
        scopeD.Dispose();
        BunnyWorldUpdateRegistry.TestHookFactory = FakeInstall;

        // Um novo módulo pode solicitar novamente a instalação depois que
        // o host voltar a aceitar hooks. Não exige carregar outro APK.
        var f = new OnlyInput();
        f.Load(scopeE);
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 1,
            "Hook não foi recuperado na inscrição seguinte");
        BunnyWorldUpdateRegistry.DetachScope(scopeE);
        e.Unload();
        f.Unload();
        scopeE.Dispose();
        Check(BunnyWorldUpdateRegistry.ActiveHookCount == 0,
            "Hook do teste de falha não foi desmontado");
        Check(Hooks.TrueForAll(h => h.Hook.IsDisposed),
            "Handle órfão após recuperação do host");
        BunnyWorldUpdateRegistry.TestHookFactory = null;

        VerifyStandaloneModule();

        Console.WriteLine("SystemUpdateLoader C#: " + assertions +
            " verificações aprovadas; quinze estágios, três taxas e múltiplos módulos simulados, sem APK.");
    }
}
