using BunnyLoader.Source;
using System.Threading;

namespace ExampleModCSharp;

/// <summary>
/// Segundo sistema no mesmo pacote. Dois mods C# também compartilham os
/// hooks físicos: o registry chama os inscritos na ordem de carregamento.
/// </summary>
public sealed class ExampleDustAndInvasionSystem : BunnyWorldUpdateLoader
{
    protected override bool EnableUpdates => false;

    private long dustFrames;
    private long invasionFrames;
    private long inputFrames;

    protected override void PreUpdateDusts()
    {
        // Antes de Dust.UpdateDust; prepare decisões sem criar partículas aqui.
    }

    protected override void PostUpdateDusts()
    {
        // Após Dust.UpdateDust. Este contador não cria trabalho gráfico.
        Interlocked.Increment(ref dustFrames);
    }

    protected override void PreUpdateInvasions()
    {
        // Antes da rotina nativa de invasões.
    }

    protected override void PostUpdateInvasions()
    {
        // Depois da rotina nativa: observe mudanças com custo constante.
        Interlocked.Increment(ref invasionFrames);
    }

    protected override void PostUpdateInput()
    {
        // O input já foi processado. Evite ações que alterem a direção da mira
        // no mesmo tick do disparo; a direção precisa acompanhar a mira.
        Interlocked.Increment(ref inputFrames);
    }

    public override string DiagnosticStatus()
    {
        return base.DiagnosticStatus() +
            " poeira=" + Interlocked.Read(ref dustFrames) +
            " invasoes=" + Interlocked.Read(ref invasionFrames) +
            " input=" + Interlocked.Read(ref inputFrames);
    }
}
