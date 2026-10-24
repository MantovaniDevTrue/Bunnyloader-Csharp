using System;

namespace BunnyLoader.Source;

public readonly struct BunnyWorldContext
{
    public long Session { get; }
    public int NetMode { get; }
    public int ElapsedFrames { get; }
    public int PlayerIndex { get; }
    public float PlayerX { get; }
    public float PlayerY { get; }
    public bool IsAuthoritative => NetMode == 0 || NetMode == 2 || NetMode == 3;

    internal BunnyWorldContext(long session, int netMode, int elapsedFrames,
        int playerIndex, float playerX, float playerY)
    {
        Session = session;
        NetMode = netMode;
        ElapsedFrames = elapsedFrames;
        PlayerIndex = playerIndex;
        PlayerX = playerX;
        PlayerY = playerY;
    }
}

public abstract class BunnyWorldSystem
{
    public virtual void ClearWorld() { }
    public virtual void OnWorldLoad() { }
    public virtual void LoadWorldData(BunnyWorldTag tag) { }
    public virtual void PostWorldLoad() { }
    public virtual void SaveWorldData(BunnyWorldTag tag) { }
    public virtual void OnWorldUnload() { }
    public virtual void NetSend(System.IO.BinaryWriter writer) { }
    public virtual void NetReceive(System.IO.BinaryReader reader) { }

    // Both public APIs share one base type, but each instance belongs to one lifecycle.
    internal static bool UsesEngineLifecycle(Type type)
    {
        bool engine = false, legacy = false;
        foreach (var method in type.GetMethods()) {
            if (method.DeclaringType == typeof(BunnyWorldSystem) ||
                method.GetBaseDefinition().DeclaringType != typeof(BunnyWorldSystem)) continue;
            var parameters = method.GetParameters();
            if (method.Name is "OnWorldEnter" or "OnWorldTick" or "OnWorldExit") engine = true;
            if (method.Name is "OnWorldLoad" or "PostWorldLoad" or "OnWorldUnload") legacy = true;
            foreach (var parameter in parameters) {
                if (parameter.ParameterType == typeof(BunnyWorldData) ||
                    parameter.ParameterType == typeof(BunnyNetWriter) ||
                    parameter.ParameterType == typeof(BunnyNetReader)) engine = true;
                if (parameter.ParameterType == typeof(BunnyWorldTag) ||
                    parameter.ParameterType == typeof(System.IO.BinaryWriter) ||
                    parameter.ParameterType == typeof(System.IO.BinaryReader)) legacy = true;
            }
        }
        if (engine && legacy) throw new InvalidOperationException("Mixed world-system lifecycle: " + type.FullName);
        return engine;
    }

    public virtual void SaveWorldData(BunnyWorldData tag) { }
    public virtual void LoadWorldData(BunnyWorldData tag) { }
    public virtual void NetSend(BunnyNetWriter writer) { }
    public virtual void NetReceive(BunnyNetReader reader) { }
    public virtual void OnWorldEnter(in BunnyWorldContext world) { }
    public virtual void OnWorldTick(in BunnyWorldContext world) { }
    public virtual void OnWorldExit() { }
}
