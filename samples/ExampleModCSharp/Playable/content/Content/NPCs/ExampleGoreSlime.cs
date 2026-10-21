using BunnyLoader.Source;

namespace ExampleModCSharp;

/// <summary>
/// Exemplo completo do GoreLoader: autoload dos PNGs, resolução pelo UID,
/// configuração das tabelas e fragmentos físicos na morte de um NPC próprio.
/// Os fragmentos são os assets originais do ExampleModBunny; isso demonstra
/// o loader de Gore, sem classificar o ExampleBoss como já portado.
/// </summary>
public sealed class ExampleGoreSlime : BunnyNPC
{
    private const string Front = "ExampleBoss_GoreFront";
    private const string Back = "ExampleBoss_GoreBack";
    private bool configured;

    public override void SetDefaults()
    {
        DisplayName = "Example Gore Slime";
        Texture = "Content/NPCs/ExampleSlimeNPC.png";
        Width = 32;
        Height = 24;
        LifeMax = 45;
        Damage = 8;
        Defense = 3;
        AiStyle = 1;
        AnimationType = 1;
        Frames = 2;
        // AiStyle 1 usa salto, gravidade e colisão do próprio Terraria.
        // Nenhuma varredura ou emissão de partículas roda por tick no Mono.
    }

    public override void OnSpawn(ref BunnyNpcAiState state)
    {
        if (configured) return;
        // ModId é atribuído pelo host, não é o nome exibido nem um ID de tile.
        // SpecialAI 0 escolhe o fragmento comum do Terraria. Velocidades 1
        // preservam o desaparecimento nativo; SafeGore inclui o filtro infantil.
        var settings = new BunnyGoreSettings(specialAI: 0, disappearSpeed: 1,
            disappearSpeedAlpha: 1, isDrip: false, safeGore: true);
        bool front = BunnyGore.TryConfigure(ModId, Front, settings);
        bool back = BunnyGore.TryConfigure(ModId, Back, settings);
        configured = front && back;
        // As recusas conservam o motivo em BunnyGore.LastError e no relatório.
        // configured só fica true quando os dois pedidos entram na fila.
    }

    public override void OnKill(ref BunnyNpcAiState state)
    {
        float centerX = state.X + Width * 0.5f;
        float centerY = state.Y + Height * 0.5f;
        // O estado conserva o último snapshot nativo do NPC. Não use (0,0)
        // nem coordenadas da tela: NewGore recebe pixels absolutos do mundo.
        // Os impulsos opostos separam as duas partes. NewGore aplica física,
        // colisão, animação e desaparecimento, na thread do jogo.
        BunnyGore.TrySpawn(ModId, Front, centerX + 8, centerY,
            state.VelocityX + 2, state.VelocityY - 2, 1f);
        BunnyGore.TrySpawn(ModId, Back, centerX - 8, centerY,
            state.VelocityX - 2, state.VelocityY - 2, 1f);
        // true em TrySpawn significa pedido aceito; a emissão acontece no
        // próximo pump nativo. A saída do mundo cancela pedidos daquela sessão.
    }
}
