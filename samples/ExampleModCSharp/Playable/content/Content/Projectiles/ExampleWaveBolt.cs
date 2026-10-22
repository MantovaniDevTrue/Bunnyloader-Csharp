using System;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWaveBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Bolt";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 10;
        Height = 10;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 2;
        TimeLeft = 180;
    }

    public override bool AI(ref BunnyProjectileAiState state)
    {
        float vx = state.VelocityX;
        float vy = state.VelocityY;
        if (!float.IsFinite(vx) || !float.IsFinite(vy)) return false;
        float turn = MathF.Sin(state.Age * 0.12f) * 0.018f;
        float sine = MathF.Sin(turn);
        float cosine = MathF.Cos(turn);
        state.VelocityX = vx * cosine - vy * sine;
        state.VelocityY = vx * sine + vy * cosine;
        return true;
    }

    public static int HitEvents;
    public static int KillEvents;
    private static bool dropletConfigured;

    public override void OnSpawn(ref BunnyProjectileAiState state)
    {
        // No ExampleModBunny, ExampleDroplet pertence ao estilo de agua.
        // Configurar uma vez como gota segura para o filtro infantil evita
        // repetir a escrita das tabelas a cada tick ou morte de projetil.
        if (!dropletConfigured)
            dropletConfigured = BunnyGore.TryConfigure(
                "examplemodcsharp", "ExampleDroplet",
                new BunnyGoreSettings(isDrip: true, safeGore: true));
    }

    public override void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage)
    {
        if (npcIndex >= 0 && damage >= 0) HitEvents++;
    }

    public override void OnKill(ref BunnyProjectileAiState state)
    {
        KillEvents++;
        // Exemplo do PNG original ExampleDroplet em Content/Gores.
        // O centro da bala e o impulso para cima reproduzem um fragmento
        // fisico do Terraria; nao desenhamos um sprite falso no MonoVM.
        // Nao varrer NPCs nem criar efeito no AI a cada tick.
        BunnyGore.TrySpawn("examplemodcsharp", "ExampleDroplet",
            state.X + Width * 0.5f, state.Y + Height * 0.5f,
            state.VelocityX * 0.4f, -1.5f, 0.8f);
    }

}

