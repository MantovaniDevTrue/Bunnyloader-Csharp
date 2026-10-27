using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Protege um arco fixo de 90 graus, centrado no eixo positivo X (direita).
// Uso: Em ModifyHurt; o angulo recebido esta em radianos, nao em graus.
// Limite: Isto NAO acompanha a direcao do jogador; essa informacao nao esta neste contexto.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class DirectionalGuard
{
    public static int Apply(BunnyHurtModifiers hit)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return BunnyCombat.HasSpatialContext(hit) && BunnyCombat.IsInArc(hit.AttackerAngle, 0f, System.MathF.PI / 2)
            ? BunnyCombat.ReduceDamage(hit.Damage, 20) : hit.Damage;
    }
}

