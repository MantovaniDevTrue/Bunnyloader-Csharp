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
