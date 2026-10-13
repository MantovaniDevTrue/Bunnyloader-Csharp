using System;

namespace BunnyLoader.Source;

/// <summary>Origem capturada pelo jogo. Unknown inclui dano ambiental; nao significa "NPC 0".</summary>
public enum BunnyHurtSource { Unknown, Npc, Projectile }

/// <summary>
/// Operacoes puras para callbacks de dano. Nao acessam Unity e nao alteram
/// o jogador: atribua o resultado a modifiers.Damage no callback apropriado.
/// </summary>
public static class BunnyCombat
{
    /// <summary>NPC tem prioridade, como no dispatcher nativo. Indices invalidos nao viram entidades.</summary>
    public static BunnyHurtSource SourceOf(int npcIndex, int projectileIndex) =>
        npcIndex >= 0 && npcIndex < 200 ? BunnyHurtSource.Npc :
        projectileIndex >= 0 && projectileIndex < 1000 ? BunnyHurtSource.Projectile :
        BunnyHurtSource.Unknown;

    /// <summary>Vida em 0..100; -1 se a vida maxima nao foi capturada.</summary>
    public static int LifePercent(int life, int maxLife) =>
        maxLife <= 0 ? -1 : (int)Math.Clamp((long)life * 100 / maxLife, 0, 100);

    /// <summary>Comparacao exata, sem arredondar a porcentagem antes da decisao.</summary>
    public static bool IsLowLife(int life, int maxLife, int thresholdPercent)
    {
        Percent(thresholdPercent);
        return maxLife > 0 && life >= 0 && (long)life * 100 <= (long)maxLife * thresholdPercent;
    }

    /// <summary>
    /// Reduz 0..100%, arredondando para baixo. Nunca aumenta o dano.
    /// O piso padrao e 1; use minimum=0 para permitir zero. Cancelar/esquivar
    /// e uma decisao separada (Cancel/FreeDodge), nao um efeito deste metodo.
    /// </summary>
    public static int ReduceDamage(int damage, int percent, int minimum = 1)
    {
        if (damage < 0) throw new ArgumentOutOfRangeException(nameof(damage));
        if (minimum < 0) throw new ArgumentOutOfRangeException(nameof(minimum));
        Percent(percent);
        return (int)Math.Min(damage, Math.Max(minimum, (long)damage * (100 - percent) / 100));
    }

    /// <summary>Limita um golpe, sem dano negativo ou aumento acidental.</summary>
    public static int CapDamage(int damage, int maximum)
    {
        if (damage < 0) throw new ArgumentOutOfRangeException(nameof(damage));
        if (maximum < 0) throw new ArgumentOutOfRangeException(nameof(maximum));
        return Math.Min(damage, maximum);
    }

    /// <summary>Distancia em pixels. -1/NaN/infinito indicam contexto indisponivel.</summary>
    public static bool IsWithinDistance(float distance, float maximumPixels)
    {
        NonNegativeFinite(maximumPixels, nameof(maximumPixels));
        return float.IsFinite(distance) && distance >= 0 && distance <= maximumPixels;
    }

    /// <summary>Velocidade em pixels por tick; componentes negativas sao validas.</summary>
    public static float Speed(float velocityX, float velocityY)
    {
        if (!float.IsFinite(velocityX) || !float.IsFinite(velocityY)) return float.NaN;
        double speed = Math.Sqrt((double)velocityX * velocityX + (double)velocityY * velocityY);
        return speed <= float.MaxValue ? (float)speed : float.NaN;
    }

    /// <summary>Normaliza radianos em [-PI, PI]. Contexto ausente deve ser filtrado antes.</summary>
    public static float NormalizeAngle(float radians)
    {
        if (!float.IsFinite(radians)) return float.NaN;
        return (float)Math.IEEERemainder(radians, 2 * Math.PI);
    }

    /// <summary>Compara um angulo em radianos com um arco de largura total em radianos.</summary>
    public static bool IsInArc(float angle, float center, float width)
    {
        if (!float.IsFinite(width) || width < 0 || width > 2 * MathF.PI)
            throw new ArgumentOutOfRangeException(nameof(width));
        if (!float.IsFinite(angle) || !float.IsFinite(center)) return false;
        return Math.Abs(Math.IEEERemainder((double)angle - center, 2 * Math.PI)) <= width / 2;
    }

    /// <summary>
    /// Distancia valida e o marcador de presenca: X/Y/velocidade/angulo
    /// podem legitimamente valer -1. Nunca use AttackerX >= 0 como teste.
    /// </summary>
    public static bool HasSpatialContext(BunnyHurtModifiers hit)
    {
        if (hit is null) throw new ArgumentNullException(nameof(hit));
        return Spatial(hit.NpcIndex, hit.ProjectileIndex, hit.AttackerDistance,
            hit.AttackerX, hit.AttackerY, hit.AttackerVelocityX, hit.AttackerVelocityY, hit.AttackerAngle);
    }

    /// <summary>Mesma validacao para o snapshot recebido depois do golpe.</summary>
    public static bool HasSpatialContext(BunnyHurtInfo hit) =>
        Spatial(hit.NpcIndex, hit.ProjectileIndex, hit.AttackerDistance,
            hit.AttackerX, hit.AttackerY, hit.AttackerVelocityX, hit.AttackerVelocityY, hit.AttackerAngle);

    private static bool Spatial(int npc, int projectile, float distance,
        float x, float y, float vx, float vy, float angle) =>
        SourceOf(npc, projectile) != BunnyHurtSource.Unknown &&
        float.IsFinite(distance) && distance >= 0 && float.IsFinite(x) &&
        float.IsFinite(y) && float.IsFinite(vx) && float.IsFinite(vy) && float.IsFinite(angle);

    private static void Percent(int value)
    {
        if (value < 0 || value > 100) throw new ArgumentOutOfRangeException(nameof(value), "Use 0..100.");
    }

    private static void NonNegativeFinite(float value, string name)
    {
        if (!float.IsFinite(value) || value < 0) throw new ArgumentOutOfRangeException(name);
    }
}
