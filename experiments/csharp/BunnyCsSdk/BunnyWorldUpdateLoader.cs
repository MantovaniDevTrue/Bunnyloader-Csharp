using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using System.Runtime.CompilerServices;

[assembly: InternalsVisibleTo("WorldUpdateLoader.Tests")]

namespace BunnyLoader.Source;

[Flags]
internal enum BunnyUpdateStageFlags
{
    None = 0,
    PreUpdateEntities = 1,
    PostUpdateEverything = 2,
    PreUpdateProjectiles = 4,
    PreUpdateDusts = 8,
    PostUpdateDusts = 16,
    PreUpdateInvasions = 32,
    PostUpdateInvasions = 64,
    PostUpdateInput = 128,
    PreUpdatePlayers = 256,
    PostUpdatePlayers = 512,
    PreUpdateNPCs = 1024,
    PostUpdateNPCs = 2048,
    PreUpdateGores = 4096,
    PostUpdateGores = 8192,
    PostUpdateProjectiles = 16384,
    PreUpdateItems = 32768,
    PostUpdateItems = 65536,
    UpdateUI = 131072
}

/// <summary>
/// Eventos do mundo executados pelo MonoVM, sem JavaScript. Um único hook nativo
/// é compartilhado entre os mods, e o método original do Terraria roda uma vez.
/// Os métodos só são ligados quando uma subclasse realmente os sobrescreve.
/// </summary>
public abstract class BunnyWorldUpdateLoader : BunnyHookModule
{
    private BunnyUpdateStageFlags flags;
    private long callbacks;
    private long faults;
    private string lastError = "";
    private bool loaded;
    private int registered;

    public long CallbackCount => Interlocked.Read(ref callbacks);
    public long ErrorCount => Interlocked.Read(ref faults);
    public string LastError => Volatile.Read(ref lastError);
    public bool IsRegistered => Volatile.Read(ref registered) != 0;

    /// <summary>Exemplos podem desligar o loader sem instalar nenhum hook.</summary>
    protected virtual bool EnableUpdates => true;

    /// <summary>Antes do estágio PlayersBegin original.</summary>
    protected virtual void PreUpdatePlayers() { }
    /// <summary>Depois da rotina de jogadores, em PlayersEnd.</summary>
    protected virtual void PostUpdatePlayers() { }
    /// <summary>Antes do estágio NPCsBegin original.</summary>
    protected virtual void PreUpdateNPCs() { }
    /// <summary>Depois dos NPCs, antes dos gores, no estágio NPCsEnd.</summary>
    protected virtual void PostUpdateNPCs() { }
    /// <summary>No mesmo ponto NPCsEnd, antes da atualização de gores.</summary>
    protected virtual void PreUpdateGores() { }
    /// <summary>Depois da atualização de gores, no estágio GoresEnd.</summary>
    protected virtual void PostUpdateGores() { }
    /// <summary>No estágio ItemsBegin, após os projéteis e antes dos itens.</summary>
    protected virtual void PostUpdateProjectiles() { }
    /// <summary>No estágio ItemsBegin, imediatamente após PostUpdateProjectiles.</summary>
    protected virtual void PreUpdateItems() { }
    /// <summary>Depois dos itens, no estágio ItemsEnd.</summary>
    protected virtual void PostUpdateItems() { }

    /// <summary>Antes de Main.DoUpdateInWorld, para preparar lógica do mundo.</summary>
    protected virtual void PreUpdateEntities() { }

    /// <summary>Após Main.DoUpdateInWorld, depois da atualização das entidades.</summary>
    protected virtual void PostUpdateEverything() { }

    /// <summary>Antes de Main.PreUpdateAllProjectiles; não substitui a IA do projétil.</summary>
    protected virtual void PreUpdateProjectiles() { }

    /// <summary>Antes de Dust.UpdateDust. Evite alocações por quadro.</summary>
    protected virtual void PreUpdateDusts() { }

    /// <summary>Após Dust.UpdateDust, com o estado atualizado.</summary>
    protected virtual void PostUpdateDusts() { }

    /// <summary>Antes de Main.UpdateInvasion.</summary>
    protected virtual void PreUpdateInvasions() { }

    /// <summary>Após Main.UpdateInvasion.</summary>
    protected virtual void PostUpdateInvasions() { }

    /// <summary>Após Main.UpdateUIStates, somente durante o mundo. Handle GameTime válido neste callback.</summary>
    protected virtual void UpdateUI(nint gameTime) { }

    /// <summary>Após Main.DoUpdate_HandleInput, para ler o input já processado.</summary>
    protected virtual void PostUpdateInput() { }

    /// <summary>
    /// O dispatcher do Bunny chama Load uma vez por mod. A vinculação é feita
    /// aqui, e nunca por frame; outras subclasses dividem o mesmo hook físico.
    /// </summary>
    public sealed override void Load(BunnyHookScope hooks)
    {
        if (hooks is null) throw new ArgumentNullException(nameof(hooks));
        if (loaded) throw new InvalidOperationException("Loader de mundo registrado duas vezes");
        if (!EnableUpdates) return;
        flags = FindOverrides();
        if (flags == BunnyUpdateStageFlags.None) return;
        BunnyWorldUpdateRegistry.Attach(this, hooks);
        loaded = true;
        Volatile.Write(ref registered, 1);
    }

    /// <summary>A desmontagem libera a inscrição sem descartar outros mods ativos.</summary>
    public sealed override void Unload()
    {
        if (!loaded) return;
        loaded = false;
        Deactivate();
        BunnyWorldUpdateRegistry.Detach(this);
        OnUnloaded();
    }

    /// <summary>Limpeza opcional do mod, após cancelar sua inscrição.</summary>
    protected virtual void OnUnloaded() { }

    public override string DiagnosticStatus()
    {
        return "SystemUpdate C#: hooks=" + BunnyWorldUpdateRegistry.ActiveHookCount +
            " eventos=" + CallbackCount + " falhas=" + ErrorCount +
            " falhasInstalacao=" + BunnyWorldUpdateRegistry.InstallFaultCount +
            (LastError.Length == 0 ? "" : " ultimo=" + LastError);
    }

    private BunnyUpdateStageFlags FindOverrides()
    {
        BunnyUpdateStageFlags result = BunnyUpdateStageFlags.None;
        foreach (BunnyUpdateStageFlags flag in Enum.GetValues<BunnyUpdateStageFlags>())
        {
            if (flag == BunnyUpdateStageFlags.None) continue;
            MethodInfo? method = GetType().GetMethod(flag.ToString(), BindingFlags.Instance | BindingFlags.NonPublic);
            if (method is null || method.DeclaringType == typeof(BunnyWorldUpdateLoader)) continue;
            if (method.GetBaseDefinition().DeclaringType != typeof(BunnyWorldUpdateLoader)) continue;
            result |= flag;
        }
        return result;
    }

    internal bool Wants(BunnyUpdateStageFlags flag) => (flags & flag) != 0;

    internal void Deactivate() => Volatile.Write(ref registered, 0);

    internal void Invoke(BunnyUpdateStageFlags flag, nint context = default)
    {
        if (!IsRegistered) return;
        try
        {
            switch (flag)
            {
                case BunnyUpdateStageFlags.PreUpdatePlayers: PreUpdatePlayers(); break;
                case BunnyUpdateStageFlags.PostUpdatePlayers: PostUpdatePlayers(); break;
                case BunnyUpdateStageFlags.PreUpdateNPCs: PreUpdateNPCs(); break;
                case BunnyUpdateStageFlags.PostUpdateNPCs: PostUpdateNPCs(); break;
                case BunnyUpdateStageFlags.PreUpdateGores: PreUpdateGores(); break;
                case BunnyUpdateStageFlags.PostUpdateGores: PostUpdateGores(); break;
                case BunnyUpdateStageFlags.PostUpdateProjectiles: PostUpdateProjectiles(); break;
                case BunnyUpdateStageFlags.PreUpdateItems: PreUpdateItems(); break;
                case BunnyUpdateStageFlags.PostUpdateItems: PostUpdateItems(); break;
                case BunnyUpdateStageFlags.PreUpdateEntities: PreUpdateEntities(); break;
                case BunnyUpdateStageFlags.PostUpdateEverything: PostUpdateEverything(); break;
                case BunnyUpdateStageFlags.PreUpdateProjectiles: PreUpdateProjectiles(); break;
                case BunnyUpdateStageFlags.PreUpdateDusts: PreUpdateDusts(); break;
                case BunnyUpdateStageFlags.PostUpdateDusts: PostUpdateDusts(); break;
                case BunnyUpdateStageFlags.PreUpdateInvasions: PreUpdateInvasions(); break;
                case BunnyUpdateStageFlags.PostUpdateInvasions: PostUpdateInvasions(); break;
                case BunnyUpdateStageFlags.PostUpdateInput: PostUpdateInput(); break;
                case BunnyUpdateStageFlags.UpdateUI: UpdateUI(context); break;
            }
            Interlocked.Increment(ref callbacks);
        }
        catch (Exception error)
        {
            Interlocked.Increment(ref faults);
            string message = flag + ":" + error.GetType().Name + ":" + error.Message;
            Volatile.Write(ref lastError, message.Length <= 180 ? message : message.Substring(0, 180));
        }
    }
}

/// <summary>
/// Registry compartilhado: o hook universal nativo permite um dono ativo por
/// método. Este registry distribui os eventos a todos os mods sem registrar
/// dois hooks no mesmo endereço nem executar o Terraria duas vezes.
/// </summary>
internal static class BunnyWorldUpdateRegistry
{
    private sealed class Stage
    {
        internal readonly string Type;
        internal readonly string Method;
        internal readonly bool IsStatic;
        internal readonly BunnyUpdateStageFlags Before;
        internal readonly BunnyUpdateStageFlags After;
        internal BunnyWorldUpdateLoader[] BeforeListeners = Array.Empty<BunnyWorldUpdateLoader>();
        internal BunnyWorldUpdateLoader[] AfterListeners = Array.Empty<BunnyWorldUpdateLoader>();
        internal BunnyHookScope? Owner;
        internal BunnyMethodHook? Hook;

        internal Stage(string type, string method, bool isStatic,
            BunnyUpdateStageFlags before, BunnyUpdateStageFlags after)
        {
            Type = type;
            Method = method;
            IsStatic = isStatic;
            Before = before;
            After = after;
        }
    }

    private sealed class Subscription
    {
        internal readonly BunnyWorldUpdateLoader Loader;
        internal readonly BunnyHookScope Scope;
        internal Subscription(BunnyWorldUpdateLoader loader, BunnyHookScope scope)
        {
            Loader = loader;
            Scope = scope;
        }
    }

    private static readonly object Gate = new();
    private static readonly List<Subscription> Subscribers = new();
    private static long installFaults;
    private static string lastInstallError = "";

    internal static long InstallFaultCount => Interlocked.Read(ref installFaults);
    internal static string LastInstallError => Volatile.Read(ref lastInstallError);
    private static readonly Stage[] Stages =
    {
        new("Bunny.SystemStage", "EntitiesBegin", false,
            BunnyUpdateStageFlags.PreUpdateEntities, BunnyUpdateStageFlags.None),
        new("Bunny.SystemStage", "PlayersBegin", false,
            BunnyUpdateStageFlags.PreUpdatePlayers, BunnyUpdateStageFlags.None),
        new("Bunny.SystemStage", "PlayersEnd", false,
            BunnyUpdateStageFlags.PostUpdatePlayers, BunnyUpdateStageFlags.None),
        new("Bunny.SystemStage", "NPCsBegin", false,
            BunnyUpdateStageFlags.PreUpdateNPCs, BunnyUpdateStageFlags.None),
        new("Bunny.SystemStage", "NPCsEnd", false,
            BunnyUpdateStageFlags.PostUpdateNPCs, BunnyUpdateStageFlags.PreUpdateGores),
        new("Bunny.SystemStage", "GoresEnd", false,
            BunnyUpdateStageFlags.PostUpdateGores, BunnyUpdateStageFlags.None),
        new("Bunny.SystemStage", "ItemsBegin", false,
            BunnyUpdateStageFlags.PostUpdateProjectiles, BunnyUpdateStageFlags.PreUpdateItems),
        new("Bunny.SystemStage", "ItemsEnd", false,
            BunnyUpdateStageFlags.PostUpdateItems, BunnyUpdateStageFlags.None),
        new("Bunny.SystemStage", "EntitiesEnd", false,
            BunnyUpdateStageFlags.PostUpdateEverything, BunnyUpdateStageFlags.None),
        new("Bunny.SystemStage", "UIEnd", false,
            BunnyUpdateStageFlags.UpdateUI, BunnyUpdateStageFlags.None),
        new("Terraria.Main", "PreUpdateAllProjectiles", false,
            BunnyUpdateStageFlags.PreUpdateProjectiles, BunnyUpdateStageFlags.None),
        new("Terraria.Dust", "UpdateDust", true,
            BunnyUpdateStageFlags.PreUpdateDusts, BunnyUpdateStageFlags.PostUpdateDusts),
        new("Terraria.Main", "UpdateInvasion", true,
            BunnyUpdateStageFlags.PreUpdateInvasions, BunnyUpdateStageFlags.PostUpdateInvasions),
        new("Terraria.Main", "DoUpdate_HandleInput", false,
            BunnyUpdateStageFlags.None, BunnyUpdateStageFlags.PostUpdateInput)
    };

    // Testes .NET8 injetam handles falsos. Em produção é sempre null e os
    // hooks reais são instalados exclusivamente por BunnyHookScope.HookVoid.
    internal static Func<BunnyHookScope, string, string, BunnyVoidHook, bool, BunnyMethodHook>? TestHookFactory;

    internal static int ActiveHookCount
    {
        get
        {
            lock (Gate)
            {
                int count = 0;
                foreach (Stage stage in Stages)
                    if (stage.Hook is { IsDisposed: false }) count++;
                return count;
            }
        }
    }

    internal static void Attach(BunnyWorldUpdateLoader loader, BunnyHookScope scope)
    {
        lock (Gate)
        {
            if (Subscribers.Exists(entry => ReferenceEquals(entry.Loader, loader)))
                throw new InvalidOperationException("Loader de mundo já inscrito");
            Subscribers.Add(new Subscription(loader, scope));
            try { Rebuild(); }
            catch
            {
                Subscribers.RemoveAt(Subscribers.Count - 1);
                Rebuild();
                throw;
            }
        }
    }

    internal static void Detach(BunnyWorldUpdateLoader loader)
    {
        lock (Gate)
        {
            loader.Deactivate();
            if (Subscribers.RemoveAll(entry => ReferenceEquals(entry.Loader, loader)) != 0)
                Rebuild(tolerateInstallFailure: true);
        }
    }

    // O dispatcher deve chamar este método ANTES de Dispose no escopo.
    // Todas as classes do mesmo mod usam o mesmo escopo, logo a saída tem
    // que ser atômica; caso contrário ocorre reinstalação num escopo morto.
    internal static void DetachScope(BunnyHookScope scope)
    {
        lock (Gate)
        {
            foreach (Subscription entry in Subscribers)
                if (ReferenceEquals(entry.Scope, scope)) entry.Loader.Deactivate();
            if (Subscribers.RemoveAll(entry => ReferenceEquals(entry.Scope, scope)) != 0)
                Rebuild(tolerateInstallFailure: true);
        }
    }

    private static void Rebuild(bool tolerateInstallFailure = false)
    {
        foreach (Stage stage in Stages)
        {
            var before = new List<BunnyWorldUpdateLoader>();
            var after = new List<BunnyWorldUpdateLoader>();
            BunnyHookScope? owner = null;

            foreach (Subscription entry in Subscribers)
            {
                bool wantsBefore = stage.Before != BunnyUpdateStageFlags.None &&
                                   entry.Loader.Wants(stage.Before);
                bool wantsAfter = stage.After != BunnyUpdateStageFlags.None &&
                                  entry.Loader.Wants(stage.After);
                if (!wantsBefore && !wantsAfter) continue;
                owner ??= entry.Scope;
                if (wantsBefore) before.Add(entry.Loader);
                if (wantsAfter) after.Add(entry.Loader);
            }

            if (owner is null || !ReferenceEquals(owner, stage.Owner) ||
                stage.Hook is null || stage.Hook.IsDisposed)
            {
                stage.Hook?.Dispose();
                stage.Hook = null;
                stage.Owner = null;
                if (owner is not null)
                {
                    Stage captured = stage;
                    BunnyVoidHook handler = (original, self) => Dispatch(captured, original.Invoke, self);
                    try
                    {
                        BunnyMethodHook hook = TestHookFactory is not null
                            ? TestHookFactory(owner, stage.Type, stage.Method, handler, stage.IsStatic)
                            : stage.Type == "Bunny.SystemStage"
                                ? owner.HookSystemStage(stage.Method, handler)
                                : owner.HookVoid(stage.Type, stage.Method, handler, stage.IsStatic);
                        stage.Hook = hook;
                        stage.Owner = owner;
                    }
                    catch (Exception error)
                    {
                        string message = stage.Method + ":" + error.GetType().Name + ":" + error.Message;
                        Volatile.Write(ref lastInstallError,
                            message.Length <= 180 ? message : message.Substring(0, 180));
                        Interlocked.Increment(ref installFaults);
                        if (!tolerateInstallFailure) throw;
                    }
                }
            }

            // Publicar listeners só depois de confirmar a instalação.
            // Callback em voo usa o snapshot anterior e não bloqueia o frame.
            Volatile.Write(ref stage.BeforeListeners, before.ToArray());
            Volatile.Write(ref stage.AfterListeners, after.ToArray());
        }
    }

    private static void Dispatch(Stage stage, Action original, nint context = default)
    {
        foreach (BunnyWorldUpdateLoader loader in Volatile.Read(ref stage.BeforeListeners))
            loader.Invoke(stage.Before, context);

        if (stage.Type == "Bunny.SystemStage")
        {
            // Um estágio é um ponto preciso DENTRO do método nativo, e não um
            // método a executar pelo Mono. Ambos os callbacks acontecem
            // antes de liberar o trampolim ARM64 para o código seguinte.
            foreach (BunnyWorldUpdateLoader loader in Volatile.Read(ref stage.AfterListeners))
                loader.Invoke(stage.After, context);
            original();
            return;
        }

        try { original(); }
        finally
        {
            foreach (BunnyWorldUpdateLoader loader in Volatile.Read(ref stage.AfterListeners))
                loader.Invoke(stage.After, context);
        }
    }

    // Só para o projeto de teste do SDK: executa o mesmo despacho gerenciado
    // com um original simulado. Nunca é chamado pelo jogo.
    internal static void TestFire(string method, Action original)
    {
        if (method == "DoUpdateInWorld")
        {
            TestFire("EntitiesBegin", () => { });
            original();
            TestFire("EntitiesEnd", () => { });
            return;
        }
        foreach (Stage stage in Stages)
            if (stage.Method == method)
            {
                Dispatch(stage, original);
                return;
            }
        throw new ArgumentException("Estágio inexistente", nameof(method));
    }
}
