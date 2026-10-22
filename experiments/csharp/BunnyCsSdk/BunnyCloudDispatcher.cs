using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>
/// Lado C# do CloudLoader. Os hooks IL2CPP não rodam aqui: o host nativo envia
/// eventos pontuais ao worker MonoVM e aplica a resposta na thread de jogo.
/// O registro é limitado e por mod, sem dependência do motor JavaScript.
/// </summary>
public static class BunnyCloudDispatcher
{
    private const int MaxMods = 16;
    private const int MaxClouds = 128;

    private sealed class CloudEntry
    {
        public readonly string Name;
        public readonly BunnyCloud Cloud;
        public CloudEntry(string name, BunnyCloud cloud) { Name = name; Cloud = cloud; }
    }

    private sealed class Context
    {
        public readonly string ModId;
        public readonly Dictionary<string, CloudEntry> Named = new(StringComparer.Ordinal);
        public readonly Dictionary<int, CloudEntry> Bound = new();
        public string LastError = "";
        public int Faults;
        public int SpawnCalls;
        public int ChanceCalls;

        public Context(string modId) { ModId = modId; }
        public void Fault(CloudEntry entry, string phase, Exception ex)
        {
            Faults++;
            LastError = entry.Name + "." + phase + ": " +
                ex.GetType().Name + ": " + ex.Message;
        }
    }

    private static readonly Dictionary<string, Context> Mods =
        new(StringComparer.Ordinal);

    public static int ModCount => Mods.Count;
    public static int BoundCloudCount => Mods.Values.Sum(x => x.Bound.Count);
    public static string LastRegistrationError { get; private set; } = "";

    /// <summary>
    /// O carregador passa o assembly já compilado pelo Roslyn. O registro falha
    /// inteiro se um conteúdo não puder ser construído, evitando IDs parciais.
    /// Chamar somente no worker, na etapa de carga, antes de instalar assets.
    /// </summary>
    public static bool Register(string modId, Assembly assembly)
    {
        LastRegistrationError = "";
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 128 ||
            assembly is null || Mods.ContainsKey(modId) || Mods.Count >= MaxMods)
            return Fail("ModId/assembly inválido, repetido ou limite de mods excedido");
        Context prepared = new(modId);
        try
        {
            Type[] types = assembly.GetTypes()
                .Where(t => t.IsClass && !t.IsAbstract &&
                    !t.ContainsGenericParameters && typeof(BunnyCloud).IsAssignableFrom(t))
                .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();

            if (types.Length == 0 || types.Length > MaxClouds)
                return Fail("Quantidade de nuvens fora de 1..128");

            foreach (Type type in types)
            {
                // Reproduz o padrão de registro nominal do ExampleModBunny,
                // sem aceitar classes com o mesmo nome em namespaces diferentes.
                if (type.GetConstructor(Type.EmptyTypes) is null)
                    return Fail(type.FullName + ": construtor público sem argumentos obrigatório");
                if (Activator.CreateInstance(type) is not BunnyCloud cloud)
                    return Fail(type.FullName + ": falha ao construir nuvem");
                cloud.SetDefaults();
                cloud.Validate();
                cloud.SetStaticDefaults();
                if (!prepared.Named.TryAdd(type.Name, new CloudEntry(type.Name, cloud)))
                    return Fail("Nome duplicado de nuvem: " + type.Name);
            }

            Mods.Add(modId, prepared);
            return true;
        }
        catch (Exception ex)
        {
            return Fail(ex.GetType().Name + ": " + ex.Message);
        }
    }

    private static bool Fail(string reason)
    {
        LastRegistrationError = reason;
        return false;
    }

    /// <summary>
    /// Vincular somente depois que TextureAssets.Cloud e CloudMasks estiverem
    /// instalados e validados. Não reutilizar IDs de outro mod nem reatribuir
    /// o mesmo ID para outra classe.
    /// </summary>
    /// <summary>
    /// MonoVM recebe somente o caminho do DLL compilado pelo Roslyn. Ele
    /// executa Assembly.LoadFrom no próprio worker; a thread do Terraria
    /// nunca precisa criar objetos do assembly gerenciado.
    /// </summary>
    public static bool RegisterFromPath(string modId, string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(assemblyPath) ||
            assemblyPath.Length > 2048) return Fail("Caminho do assembly inválido");
        try { return Register(modId, Assembly.LoadFrom(assemblyPath)); }
        catch (Exception ex) { return Fail(ex.GetType().Name + ": " + ex.Message); }
    }

    /// <summary>
    /// Resposta simples ao host nativo no período de refresh. Zero desliga
    /// a nuvem; as entradas são ordenadas por ID e produzidas fora do frame.
    /// Não retornar JSON ou nomes traduzidos em cada tentativa de spawn.
    /// </summary>
    public static string WeightsWire(string modId, bool gameMenu,
        bool tenthAnniversary, string activeBiomes)
    {
        if (!Mods.TryGetValue(modId, out Context? context) ||
            activeBiomes is null || activeBiomes.Length > 4096)
            return "";
        try
        {
            string[] biomeKeys = activeBiomes.Length == 0 ? Array.Empty<string>() :
                activeBiomes.Split(';', StringSplitOptions.RemoveEmptyEntries |
                    StringSplitOptions.TrimEntries);
            if (biomeKeys.Length > 32) return "";
            BunnyCloudEnvironment environment = new(gameMenu,
                tenthAnniversary, biomeKeys);
            var rows = new StringBuilder("BUNNY_CLOUD_WEIGHTS_V1\n");
            foreach (var pair in context.Bound.OrderBy(p => p.Key))
            {
                float weight = SpawnChance(modId, pair.Key, environment);
                rows.Append(pair.Key.ToString(CultureInfo.InvariantCulture))
                    .Append('\t')
                    .Append(weight.ToString("R", CultureInfo.InvariantCulture))
                    .Append('\n');
            }
            return rows.ToString();
        }
        catch (Exception) { return ""; }
    }

    /// <summary>
    /// O worker retorna somente o valor de SpriteEffects. O hook nativo
    /// confere slot, tipo, escala e geração na thread do jogo antes de gravar
    /// cloud.spriteDir. -1 indica falha ou nuvem desregistrada.
    /// </summary>
    public static int OnSpawnWire(string modId, int nativeType, int slot,
        float scale, int spriteEffects)
    {
        return TryOnSpawn(modId, nativeType, slot, scale,
            spriteEffects, out int result) ? result : -1;
    }

    public static bool Bind(string modId, string className, int nativeType)
    {
        if (nativeType < 0 || !Mods.TryGetValue(modId, out Context? context) ||
            !context.Named.TryGetValue(className, out CloudEntry? cloud) ||
            context.Bound.ContainsKey(nativeType) ||
            context.Bound.Values.Contains(cloud))
            return false;
        foreach (Context other in Mods.Values)
            if (other.Bound.ContainsKey(nativeType)) return false;
        context.Bound.Add(nativeType, cloud);
        return true;
    }

    /// <summary>
    /// Atualizar peso quando a pool expirar, aproximadamente uma vez a cada
    /// 60 tentativas comuns. O núcleo não deve chamar esta API por frame.
    /// Em caso de exceção a nuvem afetada é isolada e recebe peso zero.
    /// </summary>
    public static float SpawnChance(string modId, int nativeType,
        BunnyCloudEnvironment environment)
    {
        if (environment is null || !Mods.TryGetValue(modId, out Context? context) ||
            !context.Bound.TryGetValue(nativeType, out CloudEntry? entry))
            return 0f;
        try
        {
            float weight = BunnyCloud.SafeSpawnChance(entry.Cloud.SpawnChance(environment));
            context.ChanceCalls++;
            return weight;
        }
        catch (Exception ex)
        {
            context.Fault(entry, nameof(SpawnChance), ex);
            return 0f;
        }
    }

    /// <summary>
    /// Snapshot de direção. No retorno, o núcleo precisa conferir novamente
    /// slot, tipo e geração antes de escrever spriteDir no Terraria.
    /// Um erro mantém o sprite vanilla e bloqueia somente a classe defeituosa.
    /// </summary>
    public static bool TryOnSpawn(string modId, int nativeType, int slot,
        float scale, int originalSpriteEffects, out int newSpriteEffects)
    {
        newSpriteEffects = originalSpriteEffects;
        if (slot < 0 || slot >= 200 || !float.IsFinite(scale) ||
            scale <= 0f || scale > 10f || (originalSpriteEffects & ~3) != 0 ||
            !Mods.TryGetValue(modId, out Context? context) ||
            !context.Bound.TryGetValue(nativeType, out CloudEntry? entry))
            return false;
        try
        {
            var snapshot = new BunnyCloudSpawn(nativeType, slot, scale,
                (BunnyCloudSpriteEffects)originalSpriteEffects);
            entry.Cloud.OnSpawn(snapshot);
            if (!snapshot.HasValidVisuals)
                throw new InvalidOperationException("SpriteEffects inválido");
            newSpriteEffects = (int)snapshot.SpriteEffects;
            context.SpawnCalls++;
            return true;
        }
        catch (Exception ex)
        {
            context.Fault(entry, nameof(TryOnSpawn), ex);
            return false;
        }
    }

    public static bool IsRare(string modId, int nativeType) =>
        Mods.TryGetValue(modId, out Context? c) &&
        c.Bound.TryGetValue(nativeType, out CloudEntry? e) && e.Cloud.RareCloud;

    public static string Texture(string modId, int nativeType) =>
        Mods.TryGetValue(modId, out Context? c) &&
        c.Bound.TryGetValue(nativeType, out CloudEntry? e) ? e.Cloud.Texture : "";

    /// <summary>
    /// Falhas não devem poluir o log por frame; o host consulta o diagnóstico
    /// no relatório ou no descarregamento do mod.
    /// </summary>
    public static string Diagnostics(string modId)
    {
        if (!Mods.TryGetValue(modId, out Context? c)) return "";
        return "Clouds=" + c.Named.Count + ";Bound=" + c.Bound.Count +
            ";ChanceCalls=" + c.ChanceCalls + ";SpawnCalls=" + c.SpawnCalls +
            ";Faults=" + c.Faults + ";LastError=" + c.LastError;
    }

    public static bool Unregister(string modId) => Mods.Remove(modId);

    /// <summary>Reset ao reiniciar o worker, nunca no meio do hook.</summary>
    public static void Clear() { Mods.Clear(); LastRegistrationError = ""; }
}
