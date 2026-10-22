using BunnyLoader.Source;
using System.Threading;

namespace ExampleModCSharp;

/// <summary>
/// Exemplo de ModSystem adaptado para o SDK C# do Bunny.
/// A API é BunnyWorldUpdateLoader, não a classe ModSystem do tModLoader.
/// </summary>
public sealed class ExampleWorldUpdateCounter : BunnyWorldUpdateLoader
{
    // Este e o exemplo ativo do loader: dois contadores sem alocacoes por tick.
    // Os demais exemplos que alteram taxas do mundo continuam opt-in.
    protected override bool EnableUpdates => true;

    private long worldTicks;
    private long projectileTicks;

    protected override void PostUpdateEverything()
    {
        Interlocked.Increment(ref worldTicks);
        // O Terraria já executou seu update. Nunca chame esse método manualmente.
    }

    protected override void PreUpdateProjectiles()
    {
        // Ideal para atualizar o estado compartilhado de projéteis antes da IA.
        // Não percorra os 1000 projéteis a cada frame; prefira hooks do projétil.
        Interlocked.Increment(ref projectileTicks);
    }

    public override string DiagnosticStatus()
    {
        return base.DiagnosticStatus() +
            " mundo=" + Interlocked.Read(ref worldTicks) +
            " projeteis=" + Interlocked.Read(ref projectileTicks);
    }

    protected override void OnUnloaded()
    {
        Interlocked.Exchange(ref worldTicks, 0);
        Interlocked.Exchange(ref projectileTicks, 0);
    }
}
