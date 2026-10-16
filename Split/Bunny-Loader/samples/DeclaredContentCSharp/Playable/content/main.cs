using BunnyLoader.Source;

namespace DeclaredContentCSharp;

public abstract class SharedMaterial<T> : BunnyItem
{
    public override void SetDefaults()
    {
        MaxStack = 9999;
        Value = 100;
    }
}
