using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reflection;

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

    private static readonly IReadOnlyList<DamageClass> ReadOnlyClasses = new ReadOnlyCollection<DamageClass>(DamageClasses);
    private static readonly Dictionary<System.Type, DamageClass> Instances = new();
    private static readonly int DefaultClassCount = DamageClasses.Count;
    private static bool[,] effectInheritanceCache = new bool[0, 0];
    private static int generation;

    public static int DamageClassCount => DamageClasses.Count;
    public static IReadOnlyList<DamageClass> All => ReadOnlyClasses;
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
            unchecked { generation++; }
            return incoming.Count;
        }
        catch
        {
            // Não manter IDs publicados de um assembly parcialmente inválido.
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


    /// <summary>Retira classes de mods ao descarregar; os IDs vanilla permanecem estáveis.</summary>
    public static void Unload()
    {
        for (int i = DamageClasses.Count - 1; i >= DefaultClassCount; i--)
        {
            DamageClass cls = DamageClasses[i];
            Instances.Remove(cls.GetType());
            cls.Type = -1;
            DamageClasses.RemoveAt(i);
        }
        RebuildEffectInheritanceCache();
        unchecked { generation++; }
    }

    public static DamageClass? GetDamageClass(int type) =>
        type >= 0 && type < DamageClasses.Count ? DamageClasses[type] : null;

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
        if (id < 0 || id >= DamageClasses.Count || !ReferenceEquals(DamageClasses[id], damageClass))
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
            for (int j = 0; j < count; j++)
                next[i, j] = i == j || DamageClasses[i].GetEffectInheritance(DamageClasses[j]);
        effectInheritanceCache = next;
    }
}
