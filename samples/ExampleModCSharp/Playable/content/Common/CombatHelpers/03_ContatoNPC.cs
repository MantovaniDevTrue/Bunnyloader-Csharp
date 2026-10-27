using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Reduz somente contato de NPC. Indice 0 e valido; -1 e ausente.
// Uso: Use em ModifyHurt OU ModifyHitByNPC, nao nos dois para o mesmo efeito.
// Limite: NpcIndex e slot temporario; NpcType identifica a especie. Nao confunda.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class NpcContactGuard
{
    public static int Apply(BunnyHurtModifiers hit)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return BunnyCombat.SourceOf(hit.NpcIndex, hit.ProjectileIndex) == BunnyHurtSource.Npc
            ? BunnyCombat.ReduceDamage(hit.Damage, 20) : hit.Damage;
    }
}

