using System.Threading;
using BunnyLoader.Source;

namespace FiveLoadersCSharp;

public sealed class QaBolt : BunnyProjectile
{
    public static int GoreQueued, GoreRejected;
    public override void SetDefaults()
    {
        DisplayName = "QA5 Bolt";
        Texture = "Content/Projectiles/QaBolt.png";
        Width = 10;
        Height = 10;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 1;
        TimeLeft = 90;
    }
    public override void OnSpawn(ref BunnyProjectileAiState state)
    {
        // UID igual ao manifest.json. O id textual não é uma identidade de
        // conteúdo nativo; usar o UID impede colisões com outro pacote.
        BunnyGore.TryConfigure("fd8b8d75-62c9-4ae9-9000-45d9be591501", "QaFragment",
            new BunnyGoreSettings(safeGore: true));
    }
    public override void OnKill(ref BunnyProjectileAiState state)
    {
        // O worker enfileira apenas números. NewGore roda na thread do jogo.
        bool queued = BunnyGore.TrySpawn("fd8b8d75-62c9-4ae9-9000-45d9be591501", "QaFragment",
            state.X + 5, state.Y + 5, state.VelocityX * 0.2f, -2f, 1f);
        if (queued) Interlocked.Increment(ref GoreQueued);
        else Interlocked.Increment(ref GoreRejected);
    }
}
