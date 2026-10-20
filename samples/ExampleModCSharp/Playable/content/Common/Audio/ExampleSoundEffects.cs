using BunnyLoader.Source;

namespace ExampleModCSharp;

/// <summary>
/// Exemplo independente do SoundLoader nativo Bunny C#.
/// O áudio vem do próprio ExampleModBunny em Assets/Sounds/Items/Guns.
/// Registrar uma vez evita abrir, validar ou decodificar o arquivo a cada colisão.
/// </summary>
public static class ExampleSoundEffects
{
    private const string ModUid = "68c3115f-62f4-49f2-a56b-388256781d5f";
    private static BunnySoundClip? hit;
    private static BunnySoundPlayback last;

    public static void PlayHit(int worldX, int worldY)
    {
        hit ??= BunnySounds.Register(ModUid,
            new BunnySoundStyle("Assets/Sounds/Items/Guns/ExampleGun.ogg",
                volume: 0.7f,
                pitchVariance: 0.12f,
                maxInstances: 3,
                limitBehavior: BunnySoundLimitBehavior.IgnoreNew));

        // A fila executa a reprodução na thread do Terraria. A coordenada
        // é absoluta no mundo, em pixels, para atenuação e estéreo corretos.
        last = hit.Value.Play(worldX, worldY);
    }

    public static int LastPlaybackToken => last.Token;

    public static bool StopLast() => last.Stop();

    /// <summary>
    /// Exemplo de limpeza em descarregamento explícito do mod (não chamar
    /// a cada troca de mundo). O host cancela pedidos e libera estilos
    /// daquele UID sem tocar os sons pertencentes a outros mods.
    /// </summary>
    public static void Unload()
    {
        BunnySounds.UnloadMod(ModUid);
        hit = null;
        last = default;
    }
}
