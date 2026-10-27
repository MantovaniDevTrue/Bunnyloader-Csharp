using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Monta uma descricao do resultado, sem aplicar dano novamente.
// Uso: Em OnHurt: guarde a string para consulta; evite imprimir todo frame.
// Limite: OnHurt observa o golpe concluido. ModifyHurt e que decide o dano antes dele.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class PostHitSummary
{
    public static string Describe(BunnyHurtInfo hit)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return "origem=" + BunnyCombat.SourceOf(hit.NpcIndex, hit.ProjectileIndex) +
            " danoAplicado=" + hit.Damage + " vidaRestante=" + hit.LifeRemaining;
    }
}

