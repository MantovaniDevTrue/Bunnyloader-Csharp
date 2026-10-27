using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Protege somente com 30% de vida ou menos; maxLife ausente nao ativa.
// Uso: Em ModifyHurt, depois de testar accessoryEquipped e PvP.
// Limite: Nao altera CurrentLife/MaxLife: sao contexto, nao comandos para curar.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class LowLifeGuard
{
    public static int Apply(BunnyHurtModifiers hit)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return BunnyCombat.IsLowLife(hit.CurrentLife, hit.MaxLife, 30)
            ? BunnyCombat.ReduceDamage(hit.Damage, 50) : hit.Damage;
    }
}

