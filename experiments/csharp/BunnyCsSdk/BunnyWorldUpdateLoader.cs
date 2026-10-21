using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;

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
    PostUpdateInput = 128
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
    private bool attached;

    public long CallbackCount => Interlocked.Read(ref callbacks);
    public long ErrorCount => Interlocked.Read(ref faults);
    public string LastError => Volatile.Read(ref lastError);
    public bool IsRegistered => attached;

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

    /// <summary>Após Main.DoUpdate_HandleInput, para ler o input já processado.</summary>
    protected virtual void PostUpdateInput() { }

    /// <summary>
    /// O dispatcher do Bunny chama Load uma vez por mod. A vinculação é feita
    /// aqui, e nunca por frame; outras subclasses dividem o mesmo hook físico.
    /// </summary>
    public sealed override void Load(BunnyHookScope hooks)
    {
        if (hooks is null) throw new ArgumentNullException(nameof(hooks));
        if (attached) throw new InvalidOperationException("Loader de mundo registrado duas vezes");
        flags = FindOverrides();
        if (flags == BunnyUpdateStageFlags.None) return;
        BunnyWorldUpdateRegistry.Attach(this, hooks);
        attached = true;
    }

    /// <summary>A desmontagem libera a inscrição e migra o hook para outro mod ativo.</summary>
    public sealed override void Unload()
    {
        if (!attached) return;
        attached = false;
        BunnyWorldUpdateRegistry.Detach(this);
        OnUnloaded();
    }

    /// <summary>Limpeza opcional do mod, após cancelar sua inscrição.</summary>
    protected virtual void OnUnloaded() { }

    public override string DiagnosticStatus()
    {
        return "SystemUpdate C#: hooks=" + BunnyWorldUpdateRegistry.ActiveHookCount +
            " eventos=" + CallbackCount + " falhas=" + ErrorCount +
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

    internal void Invoke(BunnyUpdateStageFlags flag)
    {
        try
        {
            switch (flag)
            {
                case BunnyUpdateStageFlags.PreUpdateEntities: PreUpdateEntities(); break;
                case BunnyUpdateStageFlags.PostUpdateEverything: PostUpdateEverything(); break;
                case BunnyUpdateStageFlags.PreUpdateProjectiles: PreUpdateProjectiles(); break;
                case BunnyUpdateStageFlags.PreUpdateDusts: PreUpdateDusts(); break;
                case BunnyUpdateStageFlags.PostUpdateDusts: PostUpdateDusts(); break;
                case BunnyUpdateStageFlags.PreUpdateInvasions: PreUpdateInvasions(); break;
                case BunnyUpdateStageFlags.PostUpdateInvasions: PostUpdateInvasions(); break;
                case BunnyUpdateStageFlags.PostUpdateInput: PostUpdateInput(); break;
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
    private static readonly Stage[] Stages =
    {
        new("Terraria.Main", "DoUpdateInWorld", false,
            BunnyUpdateStageFlags.PreUpdateEntities, BunnyUpdateStageFlags.PostUpdateEverything),
        new("Terraria.Main", "PreUpdateAllProjectiles", false,
            BunnyUpdateStageFlags.PreUpdateProjectiles, BunnyUpdateStageFlags.None),
        new("Terraria.Dust", "UpdateDust", true,
            BunnyUpdateStageFlags.PreUpdateDusts, BunnyUpdateStageFlags.PostUpdateDusts),
        new("Terraria.Main", "UpdateInvasion", true,
            BunnyUpdateStageFlags.PreUpdateInvasions, BunnyUpdateStageFlags.PostUpdateInvasions),
        new("Terraria.Main", "DoUpdate_HandleInput", false,
            BunnyUpdateStageFlags.None, BunnyUpdateStageFlags.PostUpdateInput)
    };

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
            if (Subscribers.Count == 0) return;
            bool wasOwner = ReferenceEquals(Subscribers[0].Loader, loader);
            Subscribers.RemoveAll(item => ReferenceEquals(item.Loader, loader));
            if (wasOwner)
            {
                // O BunnyUniversalHookDispatcher já descartou o scope antigo.
                // Os novos hooks pertencem ao próximo mod, nunca ao descarregado.
                foreach (Stage stage in Stages)
                {
                    stage.Hook?.Dispose();
                    stage.Hook = null;
                }
            }
            Rebuild();
        }
    }

    private static void Rebuild()
    {
        foreach (Stage stage in Stages)
        {
            var before = new List<BunnyWorldUpdateLoader>();
            var after = new List<BunnyWorldUpdateLoader>();
            foreach (Subscription entry in Subscribers)
            {
                if (entry.Loader.Wants(stage.Before)) before.Add(entry.Loader);
                if (entry.Loader.Wants(stage.After)) after.Add(entry.Loader);
            }
            Volatile.Write(ref stage.BeforeListeners, before.ToArray());
            Volatile.Write(ref stage.AfterListeners, after.ToArray());

            bool required = before.Count != 0 || after.Count != 0;
            if (!required)
            {
                stage.Hook?.Dispose();
                stage.Hook = null;
                continue;
            }
            if (stage.Hook is { IsDisposed: false }) continue;
            BunnyHookScope scope = Subscribers[0].Scope;
            Stage captured = stage;
            stage.Hook = scope.HookVoid(stage.Type, stage.Method,
                (original, self) => Dispatch(captured, original), stage.IsStatic);
        }
    }

    private static void Dispatch(Stage stage, BunnyOriginalVoid original)
    {
        // Os arrays são cópias imutáveis da inscrição; sem lock por frame.
        // Uma falha de callback não suprime o update vanilla.
        foreach (BunnyWorldUpdateLoader loader in Volatile.Read(ref stage.BeforeListeners))
            loader.Invoke(stage.Before);

        try { original.Invoke(); }
        finally
        {
            foreach (BunnyWorldUpdateLoader loader in Volatile.Read(ref stage.AfterListeners))
                loader.Invoke(stage.After);
        }
    }
}
