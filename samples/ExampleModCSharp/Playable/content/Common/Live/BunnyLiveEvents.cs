// ExampleMod C# | Common/Live/BunnyLiveEvents.cs
// Callback descoberto pelo nome da classe/metodo. Preserve as assinaturas ao adaptar este exemplo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
namespace BunnyLoader.Source;

public static class BunnyLiveEvents
{
    private static int hits;
    private static int kills;

    public static int OnHitNPC(int call, int damage)
    {
        hits++;
        return damage + (call % 7) + hits;
    }

    public static int OnKill(int call, int damage)
    {
        kills++;
        return damage + (call % 5) + kills;
    }
}

