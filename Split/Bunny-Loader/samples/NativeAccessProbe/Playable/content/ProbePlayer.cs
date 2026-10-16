using BunnyLoader.Source;

namespace NativeAccessCSharpProbe;

public sealed class ProbePlayer : BunnyPlayer
{
    public override void SetDefaults() { }
    public override void UpdateEquips(bool accessoryEquipped) { }
    public override string OnEnterWorld() => "Native Access C#: confira leituras, metodos e lotes no Editor C#.";
}
