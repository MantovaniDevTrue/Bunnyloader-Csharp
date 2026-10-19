using BunnyLoader.Source;

namespace ExampleModCSharp.Walls;

// Port do ExampleWallAdvanced.js: todas as regras abaixo rodam no núcleo
// nativo. Não há uma travessia Mono/QuickJS para cada célula iluminada.
public sealed class ExampleWallAdvanced : BunnyWall {
    public override void SetStaticDefaults() {
        Texture = "Content/Walls/ExampleWallAdvanced.png";
        DisplayName = "Example Advanced Wall";
        Housing = true; // Conta como parede segura na avaliação de moradia.
        BlendWallName = "CogWall"; // O núcleo resolve o WallID real do Android.
        DustName = "Stone";
        AddMapEntry(68, 68, 68, "Example Advanced Wall");

        // Equivale a CreateDust/NumDust do original: o frame zero solta
        // esmeralda, os demais soltam pedra; falha gera 3 partículas, quebra 10.
        SetFrameDust(0, "GemEmerald");
        FailedDustCount = 3;
        DustCount = 10;

        // Framing mantém a conexão com as paredes vizinhas. Somente quando o
        // motor pede novos frames, 75% dos frames zero viram 1 ou 2.
        RandomizeFrame(0, 0.75f, 1, 2);
        AnimationFrames = 2;
        AnimationFrameTicks = 5;

        // O padrão diurno é RGB zero. A iluminação noturna mantém os valores
        // originais e entra na coleta nativa de luz, sem desenhar uma camada.
        NightLight = new BunnyWorldLight(0.1f, 0.5f, 0f);
    }
}
