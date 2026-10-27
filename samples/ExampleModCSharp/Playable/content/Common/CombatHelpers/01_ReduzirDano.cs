using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Reduz em 25%, arredondando para baixo. 40 vira 30; 1 vira 0.
// Uso: Em ModifyHurt: modifiers.Damage = ReduceHit.Apply(modifiers.Damage).
// Limite: minimum: 0 preserva a politica do ExampleVital; zero nao e FreeDodge.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class ReduceHit
{
    public static int Apply(int damage)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return BunnyCombat.ReduceDamage(damage, 25, minimum: 0);
    }
}

