// ExampleMod C# | Common/Live/ExampleWaveBoltLiveEvents.cs
// Callback descoberto pelo nome da classe/metodo. Preserve as assinaturas ao adaptar este exemplo.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Ajudantes de combate: Common/CombatHelpers/.
namespace BunnyLoader.Source;

public static class ExampleWaveBoltLiveEvents
{
    private static int hits;
    private static int kills;

    public static int OnHitNPC(int call, int damage)
    {
        hits++;
        return 1000 + damage + (call % 11) + hits;
    }

    public static int OnHitNPCV2(int call, int damage, int npcIndex)
    {
        hits++;
        return 10000 + damage + (call % 11) + hits + npcIndex;
    }

    public static int OnHitNPCV3(int call, int damage, int npcIndex, int npcType, int npcLife, float posX, float posY)
    {
        hits++;
        return 20000 + damage + (call % 11) + hits + npcIndex + npcType + npcLife + (int)posX + (int)posY;
    }

    public static int OnHitNPCV4(int call, int damage, int npcIndex, int npcType,
        int npcLife, float npcX, float npcY, int playerIndex, int playerLife,
        int playerLifeMax, float playerX, float playerY)
    {
        hits++;
        int playerHealthPercent = playerLifeMax > 0
            ? playerLife * 100 / playerLifeMax
            : 0;
        float dx = npcX - playerX;
        int enemyIsClose = dx > -160f && dx < 160f ? 1 : 0;
        if (hits % 4 == 0) {
            int direction = npcX < playerX ? 1 : 2;
            int strengthTenths = playerHealthPercent < 35 ? 55 : 35;
            return 2000000 + direction * 100 + strengthTenths;
        }
        int buffId = playerHealthPercent < 35 ? 39 : enemyIsClose == 1 ? 44 : 24;
        int durationTicks = npcLife > 100 ? 240 : 120;
        return 1000000 + buffId * 1000 + durationTicks;
    }

    public static int OnKill(int call, int damage)
    {
        kills++;
        return 2000 + damage + (call % 13) + kills;
    }
}

