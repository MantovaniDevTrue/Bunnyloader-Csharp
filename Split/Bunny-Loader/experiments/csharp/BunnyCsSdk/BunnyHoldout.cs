using System;
namespace BunnyLoader.Source;

/// <summary>Deslocamento visual da arma no referencial do jogador, aplicado após UseStyle e HoldStyle nativos.</summary>
public sealed class BunnyHoldout
{
    public float X { get; }
    public float Y { get; }
    public BunnyHoldout(float x,float y)
    {
        if(!float.IsFinite(x)||!float.IsFinite(y)||Math.Abs(x)>1024||Math.Abs(y)>1024)throw new ArgumentOutOfRangeException(nameof(x));
        X=x;Y=y;
    }
}
