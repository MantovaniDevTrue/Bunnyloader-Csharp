using BunnyLoader.Source;

namespace ExampleModCSharp.Gores;

public sealed class ExampleTree_Leaf : BunnyGore
{
    public override void SetDefaults()
    {
        Texture = "Content/Gores/ExampleTree_Leaf";
        SpecialAI = 3;
        SafeGore = true;
    }
}
