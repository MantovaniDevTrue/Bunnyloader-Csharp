// ExampleMod C# | Common/Live/ExampleLiveRuntime.cs
// Callback descoberto pelo nome da classe/metodo. Preserve as assinaturas ao adaptar este exemplo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
namespace BunnyLoader.Source;

public static class BunnyLiveEntry
{
    private static int calls;

    public static int Tick(int tick, int state)
    {
        calls++;
        return state + (tick % 7) - 3 + calls;
    }
}

