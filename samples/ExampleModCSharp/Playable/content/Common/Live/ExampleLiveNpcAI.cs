// ExampleMod C# | Common/Live/ExampleLiveNpcAI.cs
// Callback descoberto pelo nome da classe/metodo. Preserve as assinaturas ao adaptar este exemplo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
namespace BunnyLoader.Source;

public static class BunnyLiveNpc
{
    private static int updates;

    public static int AI(int playerDeltaX, int npcLife)
    {
        updates++;
        if (npcLife <= 0 || playerDeltaX >= -24 && playerDeltaX <= 24)
            return 0;
        int speed = npcLife <= 12 ? 18 : 12 + updates % 3;
        return playerDeltaX > 0 ? speed : -speed;
    }
}

