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
    public bool IsAuthoritative => NetMode != 1;

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
    public virtual void OnWorldEnter(in BunnyWorldContext world) { }
    public virtual void OnWorldTick(in BunnyWorldContext world) { }
    public virtual void OnWorldExit() { }
}
