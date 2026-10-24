using BunnyLoader.Source;
using System.Threading;

namespace ExampleModCSharp;

/// <summary>
/// Exemplo de ModSystem adaptado para o SDK C# do Bunny.
/// A API é BunnyWorldUpdateLoader, não a classe ModSystem do tModLoader.
/// </summary>
public sealed class ExampleWorldUpdateCounter : BunnyWorldUpdateLoader
{
    // Ative somente em um mundo de QA: esses callbacks podem rodar 60 vezes/s.
    // O pacote padrão não instala hooks adicionais desnecessários.
    protected override bool EnableUpdates => false;

    private long worldTicks;
    private long projectileTicks;

    protected override void PreUpdateEntities()
    {
        // Executado antes do Terraria atualizar as entidades do mundo.
        // Guarde o estado que deseja comparar no fim do mesmo update.
    }

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
