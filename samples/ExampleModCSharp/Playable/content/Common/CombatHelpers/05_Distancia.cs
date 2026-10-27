using BunnyLoader.Source;

namespace ExampleModCSharp.CombatHelpers;

// Protege de atacantes a ate 160 pixels (10 tiles de 16 pixels).
// Uso: Em ModifyHurt, somente depois do teste de equipamento desejado.
// Limite: Distancia -1/NaN nao e perto. Coordenadas negativas, por outro lado, sao validas.
// Esta receita compila no .bmod, mas NAO e um hook automatico.
// Copie a chamada para o seu BunnyPlayer; nao duplique a classe de exemplo.
public static class NearbyGuard
{
    public static int Apply(BunnyHurtModifiers hit)
    {
        // A API so calcula o resultado. O callback decide se deve aplica-lo.
        return BunnyCombat.HasSpatialContext(hit) && BunnyCombat.IsWithinDistance(hit.AttackerDistance, 160f)
            ? BunnyCombat.ReduceDamage(hit.Damage, 15) : hit.Damage;
    }
}

