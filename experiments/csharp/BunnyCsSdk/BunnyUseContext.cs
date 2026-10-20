namespace BunnyLoader.Source;

public readonly record struct BunnyUseContext(int Flags)
{
    public bool Daytime => (Flags & 1) != 0;
    public bool OnSurface => (Flags & 2) != 0;
    public bool NoVanillaInvasion => (Flags & 4) != 0;
    public bool PlayerAlive => (Flags & 8) != 0;
    public bool WorldReady => (Flags & 16) != 0;
    public bool CanStartSurfaceInvasion =>
        Daytime && OnSurface && NoVanillaInvasion && PlayerAlive && WorldReady;
}
