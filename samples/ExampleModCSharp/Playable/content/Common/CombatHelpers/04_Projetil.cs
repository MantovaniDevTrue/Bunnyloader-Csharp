using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Limita dano de projetil a 20. Golpe de 8 continua sendo 8.
// Uso: Em ModifyHurt ou ModifyHitByProjectile, com o acessorio equipado.
// Limite: Nao assume que ProjectileOwner 255 e sempre inimigo: consulte o contexto.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class ProjectileGuard
{
    public static int Apply(BunnyHurtModifiers hit)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return BunnyCombat.SourceOf(hit.NpcIndex, hit.ProjectileIndex) == BunnyHurtSource.Projectile
            ? BunnyCombat.CapDamage(hit.Damage, 20) : hit.Damage;
    }
}

