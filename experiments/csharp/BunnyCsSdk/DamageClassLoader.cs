using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reflection;
using System.Globalization;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// Registro das classes de dano. As dez classes vanilla usam os IDs 0 a 9
/// do tModLoader. A matriz de efeitos é reconstruída somente no registro,
/// nunca por ataque ou por quadro.
/// </summary>
public static class DamageClassLoader
{
    private const int MaximumClasses = 128;
    private static readonly List<DamageClass> DamageClasses = new()
    {
        DamageClass.Default, DamageClass.Generic, DamageClass.Melee,
        DamageClass.MeleeNoSpeed, DamageClass.Ranged, DamageClass.Magic,
        DamageClass.Summon, DamageClass.SummonMeleeSpeed,
        DamageClass.MagicSummonHybrid, DamageClass.Throwing
    };

    private static readonly Dictionary<System.Type, DamageClass> Instances = new();
    private static readonly Dictionary<string, List<DamageClass>> Owners = new(StringComparer.Ordinal);
    private static readonly HashSet<DamageClass> RetiredClasses = new();
    private static readonly int DefaultClassCount = DamageClasses.Count;
    private static IReadOnlyList<DamageClass> activeClasses =
        Array.AsReadOnly(Array.Empty<DamageClass>());
    private static bool[,] effectInheritanceCache = new bool[0, 0];
    private static int generation;

    /// <summary>Próximo ID. IDs de classes descarregadas não são reutilizados na sessão.</summary>
    public static int DamageClassCount => DamageClasses.Count;
    public static IReadOnlyList<DamageClass> All => activeClasses;
    public static int Generation => generation;

    static DamageClassLoader()
    {
        for (int i = 0; i < DamageClasses.Count; i++)
        {
            DamageClasses[i].Type = i;
            Instances.Add(DamageClasses[i].GetType(), DamageClasses[i]);
        }
        RebuildEffectInheritanceCache();
    }

    /// <summary>
    /// Registro durante Load do mod. O dispatcher de assembly/mod ainda deverá
    /// chamar este método de forma automática antes de carregar os itens.
    /// </summary>
    public static int Register(DamageClass damageClass)
    {
        if (damageClass is null) throw new ArgumentNullException(nameof(damageClass));
        if (damageClass is VanillaDamageClass)
            throw new ArgumentException("Classes vanilla já estão registradas", nameof(damageClass));
        if (DamageClasses.Count >= MaximumClasses)
            throw new InvalidOperationException("Limite de DamageClass atingido");
        if (damageClass.Type != -1 || Instances.ContainsKey(damageClass.GetType()))
            throw new InvalidOperationException("DamageClass já registrada: " + damageClass.GetType().FullName);

        int type = DamageClasses.Count;
        damageClass.Type = type;
        DamageClasses.Add(damageClass);
        Instances.Add(damageClass.GetType(), damageClass);
        try
        {
            RebuildEffectInheritanceCache();
            unchecked { generation++; }
            return type;
        }
        catch
        {
            Instances.Remove(damageClass.GetType());
            DamageClasses.RemoveAt(type);
            damageClass.Type = -1;
            RebuildEffectInheritanceCache();
            throw;
        }
    }

    public static int Register<T>() where T : DamageClass, new() => Register(new T());

    /// <summary>
    /// Descobre as classes de dano no assembly de um mod C# já compilado.
    /// O registro é transacional: IDs e cache só são publicados se todas as
    /// classes tiverem construtor válido e todas as heranças puderem ser lidas.
    /// O BunnyEntitySourceDispatcher chama isto uma vez por assembly carregado.
    /// </summary>
    public static int RegisterAssembly(Assembly assembly)
    {
        if (assembly is null) throw new ArgumentNullException(nameof(assembly));
        return RegisterAssembly(assembly.FullName ?? assembly.GetName().Name ?? "assembly", assembly);
    }

    public static int RegisterAssembly(string modId, Assembly assembly)
    {
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 128)
            throw new ArgumentException("ID de mod inválido", nameof(modId));
        if (assembly is null) throw new ArgumentNullException(nameof(assembly));
        if (Owners.ContainsKey(modId))
            throw new InvalidOperationException("DamageClass de mod já registrada: " + modId);
        Type[] types = assembly.GetTypes()
            .Where(t => t.IsClass && !t.IsAbstract && !t.ContainsGenericParameters &&
                typeof(DamageClass).IsAssignableFrom(t))
            .OrderBy(t => t.FullName, StringComparer.Ordinal)
            .ToArray();
        if (types.Length == 0) return 0;
        if (types.Length > MaximumClasses - DamageClasses.Count)
            throw new InvalidOperationException("Limite de classes de dano do Bunny excedido");

        var incoming = new List<DamageClass>(types.Length);
        foreach (Type type in types)
        {
            if (typeof(VanillaDamageClass).IsAssignableFrom(type))
                throw new InvalidOperationException("Não declare subclasses de VanillaDamageClass: " + type.FullName);
            if (type.GetConstructor(Type.EmptyTypes) is null)
                throw new InvalidOperationException("DamageClass exige construtor público sem argumentos: " + type.FullName);
            if (Instances.ContainsKey(type))
                throw new InvalidOperationException("DamageClass já registrada: " + type.FullName);
            if (Activator.CreateInstance(type) is not DamageClass cls || cls.Type != -1)
                throw new InvalidOperationException("Instância de DamageClass inválida: " + type.FullName);
            incoming.Add(cls);
        }

        int initial = DamageClasses.Count;
        try
        {
            foreach (DamageClass cls in incoming)
            {
                cls.Type = DamageClasses.Count;
                DamageClasses.Add(cls);
                Instances.Add(cls.GetType(), cls);
            }
            RebuildEffectInheritanceCache();
            Owners.Add(modId, incoming);
            unchecked { generation++; }
            return incoming.Count;
        }
        catch
        {
            // Não manter IDs publicados de um assembly parcialmente inválido.
            Owners.Remove(modId);
            foreach (DamageClass cls in incoming)
            {
                Instances.Remove(cls.GetType());
                cls.Type = -1;
            }
            if (DamageClasses.Count > initial)
                DamageClasses.RemoveRange(initial, DamageClasses.Count - initial);
            RebuildEffectInheritanceCache();
            throw;
        }
    }


    /// <summary>
    /// Desativa somente as classes do mod. Os slots viram lápides e jamais são
    /// reaproveitados durante a sessão; outros mods preservam seus IDs.
    /// </summary>
    public static bool UnregisterMod(string modId)
    {
        if (string.IsNullOrWhiteSpace(modId) ||
            !Owners.TryGetValue(modId, out List<DamageClass>? classes)) return false;
        foreach (DamageClass cls in classes)
        {
            RetiredClasses.Add(cls);
            Instances.Remove(cls.GetType());
        }
        try
        {
            RebuildEffectInheritanceCache();
        }
        catch
        {
            foreach (DamageClass cls in classes)
            {
                RetiredClasses.Remove(cls);
                Instances.Add(cls.GetType(), cls);
            }
            RebuildEffectInheritanceCache();
            throw;
        }
        Owners.Remove(modId);
        foreach (DamageClass cls in classes) cls.Type = -1;
        unchecked { generation++; }
        return true;
    }

    public static string ExportDefaultStatSnapshot(Assembly assembly)
    {
        if (assembly is null) throw new ArgumentNullException(nameof(assembly));
        var classes = DamageClasses.Where(cls => cls.Type >= DefaultClassCount &&
            !RetiredClasses.Contains(cls) && cls.GetType().Assembly == assembly).ToArray();
        if (classes.Length == 0) return "DAMAGE_CLASS_STATS_V1\n";
        var player = new DamageClassPlayer();
        var result = new StringBuilder("DAMAGE_CLASS_STATS_V1\n");
        static string F(float value)
        {
            if (!float.IsFinite(value) || MathF.Abs(value) > 100000f)
                throw new InvalidOperationException("DamageClass stat default outside finite supported range");
            return value.ToString("R", CultureInfo.InvariantCulture);
        }
        foreach (DamageClass cls in classes)
        {
            string name = cls.GetType().FullName ?? throw new InvalidOperationException("Unnamed DamageClass");
            if (name.Length > 240 || name.IndexOfAny(new[] { '\t', '\n', '\r' }) >= 0)
                throw new InvalidOperationException("Invalid DamageClass identity");
            StatModifier damage = player.GetTotalDamage(cls);
            StatModifier knockback = player.GetTotalKnockback(cls);
            string[] fields = {
                "CLASS", name, F(damage.Base), F(damage.Additive),
                F(damage.Multiplicative), F(damage.Flat),
                F(knockback.Base), F(knockback.Additive),
                F(knockback.Multiplicative), F(knockback.Flat),
                F(player.GetTotalCritChance(cls)),
                F(player.GetTotalAttackSpeed(cls)),
                F(player.GetTotalArmorPenetration(cls)),
                cls.UseStandardCritCalcs ? "1" : "0"
            };
            result.Append(string.Join("\t", fields)).Append('\n');
        }
        return result.ToString();
    }

    /// <summary>Reset global em shutdown; não substituir UnregisterMod no hot-reload.</summary>
    public static void Unload()
    {
        for (int i = DamageClasses.Count - 1; i >= DefaultClassCount; i--)
        {
            DamageClass cls = DamageClasses[i];
            Instances.Remove(cls.GetType());
            cls.Type = -1;
            DamageClasses.RemoveAt(i);
        }
        RetiredClasses.Clear();
        Owners.Clear();
        RebuildEffectInheritanceCache();
        unchecked { generation++; }
    }

    public static DamageClass? GetDamageClass(int type) =>
        type >= 0 && type < DamageClasses.Count && !RetiredClasses.Contains(DamageClasses[type])
            ? DamageClasses[type] : null;

    public static T GetInstance<T>() where T : DamageClass
    {
        if (!Instances.TryGetValue(typeof(T), out DamageClass? value))
            throw new InvalidOperationException("Classe não registrada: " + typeof(T).FullName);
        return (T)value;
    }

    internal static void Validate(DamageClass damageClass)
    {
        if (damageClass is null) throw new ArgumentNullException(nameof(damageClass));
        int id = damageClass.Type;
        if (id < 0 || id >= DamageClasses.Count || RetiredClasses.Contains(damageClass) ||
            !ReferenceEquals(DamageClasses[id], damageClass))
            throw new InvalidOperationException("DamageClass não registrada ou já descarregada");
    }

    internal static bool CountsAs(DamageClass source, DamageClass target)
    {
        Validate(source);
        Validate(target);
        return effectInheritanceCache[source.Type, target.Type];
    }

    private static void RebuildEffectInheritanceCache()
    {
        int count = DamageClasses.Count;
        bool[,] next = new bool[count, count];
        for (int i = 0; i < count; i++)
        {
            if (RetiredClasses.Contains(DamageClasses[i])) continue;
            for (int j = 0; j < count; j++)
            {
                if (RetiredClasses.Contains(DamageClasses[j])) continue;
                next[i, j] = i == j || DamageClasses[i].GetEffectInheritance(DamageClasses[j]);
            }
        }
        var visible = Array.AsReadOnly(DamageClasses
            .Where(cls => !RetiredClasses.Contains(cls)).ToArray());
        effectInheritanceCache = next;
        activeClasses = visible;
    }
}
