using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Protege contra velocidade >= 8 pixels por tick, em qualquer direcao.
// Uso: Em ModifyHurt; vx=-8 e tao rapido quanto vx=8.
// Limite: Speed nao distingue contexto ausente sozinho: teste HasSpatialContext antes.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class FastAttackGuard
{
    public static int Apply(BunnyHurtModifiers hit)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return BunnyCombat.HasSpatialContext(hit) && BunnyCombat.Speed(hit.AttackerVelocityX, hit.AttackerVelocityY) >= 8f
            ? BunnyCombat.ReduceDamage(hit.Damage, 10) : hit.Damage;
    }
}

