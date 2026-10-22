using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// Entrada do AchievementLoader para o worker MonoVM. Somente valores são enviados:
/// nenhuma instância ou ponteiro do Terraria atravessa a fronteira C# / ARM64.
/// A ponte nativa deve chamar Dispatch no worker existente, nunca na thread GL.
/// </summary>
public static class BunnyAchievementRuntime
{
    private sealed class ModState
    {
        public string Path = "";
        public BunnyAchievementRegistry Registry = null!;
        public BunnyAchievementStore Store = null!;
    }

    private static readonly Dictionary<string, ModState> mods = new(StringComparer.Ordinal);
    private const int MaxAssemblyBytes = 32 * 1024 * 1024;
    private const int MaxSaveBytes = 1024 * 1024;

    /// <summary>
    /// Operações:
    /// 0 = bind; 1 = morte NPC; 2 = coleta item; 3 = fabricação item;
    /// 4 = tile destruído; 5 = save; 6 = load; 7 = unload; 8 = estado da UI.
    /// Em eventos, value = tipo nativo, count = quantidade e payload = "1" apenas
    /// para eventos do jogador local. Save/load usam BACH1, independente do jogo.
    /// </summary>
    public static string Dispatch(
        string modId, string assemblyPath, int operation,
        int value, int count, string payload)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(modId) || modId.Length > 96 ||
                modId.Any(c => !char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-' && c != '.'))
                throw new ArgumentException("ModUid inválido");
            if (string.IsNullOrWhiteSpace(assemblyPath) || assemblyPath.Length > 1024)
                throw new ArgumentException("Caminho da assembly inválido");
            if (!Enum.IsDefined(typeof(BunnyAchievementOperation), operation))
                throw new ArgumentOutOfRangeException(nameof(operation));
            payload ??= "";

            // Recarregar sem unload poderia duplicar classes e perder o progresso.
            // A primeira chamada é sempre Bind; as demais reutilizam a mesma instância.
            if (operation == 0)
            {
                if (mods.TryGetValue(modId, out var previous))
                {
                    if (previous.Path != assemblyPath)
                        throw new InvalidOperationException("Assembly mudou sem Unload");
                    return Manifest(previous.Registry);
                }
                var file = new FileInfo(assemblyPath);
                if (!file.Exists || file.Length < 1 || file.Length > MaxAssemblyBytes)
                    throw new InvalidOperationException("Assembly indisponível ou maior que 32 MiB");

                Assembly assembly = Assembly.Load(File.ReadAllBytes(assemblyPath));
                Type[] types = assembly.GetTypes()
                    .Where(t => t.IsClass && !t.IsAbstract && !t.ContainsGenericParameters &&
                        typeof(BunnyAchievement).IsAssignableFrom(t))
                    .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();
                if (types.Length == 0 || types.Length > 256)
                    throw new InvalidOperationException("Esperadas entre 1 e 256 conquistas.");

                var registry = new BunnyAchievementRegistry();
                foreach (Type type in types)
                {
                    if (type.GetConstructor(Type.EmptyTypes) == null)
                        throw new InvalidOperationException(type.FullName + ": construtor público vazio obrigatório");
                    BunnyAchievement instance = (BunnyAchievement)Activator.CreateInstance(type)!;
                    registry.Register(modId, instance);
                }
                var state = new ModState {
                    Path = assemblyPath, Registry = registry,
                    Store = new BunnyAchievementStore(registry)
                };
                string manifest = Manifest(registry);
                mods.Add(modId, state);
                return manifest;
            }

            if (!mods.TryGetValue(modId, out var mod) || mod.Path != assemblyPath)
                throw new InvalidOperationException("Bind pendente ou assembly incompatível");
            BunnyAchievementRegistry achievements = mod.Registry;

            if (operation is >= 1 and <= 4)
            {
                long progressBefore = achievements.ProgressChanges;
                long completedBefore = achievements.CompletionChanges;
                if (value < (operation == 1 ? -32768 : 0) || count < 1 || count > 9999 ||
                    (payload != "0" && payload != "1"))
                    throw new ArgumentException("Evento fora do contrato");
                bool localPlayer = payload == "1";
                switch (operation)
                {
                    case 1: achievements.NotifyNPCKilled(value, localPlayer); break;
                    case 2: achievements.NotifyItemPickup(value, count, localPlayer); break;
                    case 3: achievements.NotifyItemCraft(value, count, localPlayer); break;
                    case 4: achievements.NotifyTileDestroyed(value, localPlayer); break;
                }
                mod.Store.Autosave(achievements.CompletionChanges != completedBefore);
                // Sem alterações: resposta pequena. Com progresso: publicar
                // um snapshot para o menu nativo, sem polling por quadro.
                return achievements.ProgressChanges != progressBefore
                    ? Manifest(achievements)
                    : "OK|" + achievements.CallbackFaults.ToString(CultureInfo.InvariantCulture);
            }
            if (operation == 5)
                return achievements.Save();
            if (operation == 6)
            {
                if (Encoding.UTF8.GetByteCount(payload) > MaxSaveBytes)
                    throw new ArgumentException("Save excede o limite de 1 MiB");
                achievements.Load(payload);
                mod.Store.MarkExternalRestore();
                return "OK";
            }
            if (operation == 7)
            {
                if (mod.Store.IsConfigured) mod.Store.Flush(force: true);
                string saved = achievements.Save();
                mods.Remove(modId);
                return saved;
            }
            if (operation == 8)
                return Manifest(achievements);
            if (operation == 9)
            {
                mod.Store.Configure(payload, modId);
                return "READY";
            }
            if (operation == 10)
            {
                mod.Store.Flush();
                return "OK";
            }

            throw new InvalidOperationException("Operação não implementada");
        }
        catch (Exception error)
        {
            string detail = error.GetType().Name + ": " + error.Message;
            if (detail.Length > 512) detail = detail[..512];
            return "ERROR|[CSharp][AchievementLoader] Mod=" + modId +
                " Stage=" + operation + " Detail=" + detail.Replace('\n', ' ').Replace('\r', ' ');
        }
    }

    private static string Manifest(BunnyAchievementRegistry achievements)
    {
        var output = new StringBuilder("BACH_META_V2\n");
        foreach (BunnyAchievement achievement in achievements.Achievements)
        {
            // Hex mantém nomes, descrições e caminhos com Unicode fora do protocolo.
            static string Hex(string source)
                => Convert.ToHexString(Encoding.UTF8.GetBytes(source));
            if (achievement.FriendlyName.Length > 200 ||
                achievement.Description.Length > 1000 ||
                achievement.Texture.Length > 200)
                throw new InvalidOperationException("Metadados de conquista grandes demais");
            output.Append("A|").Append(Hex(achievement.Name)).Append('|')
                .Append(Hex(achievement.FriendlyName)).Append('|')
                .Append(Hex(achievement.Description)).Append('|')
                .Append(Hex(achievement.Texture)).Append('|')
                .Append(achievement.Index).Append('|')
                .Append((int)achievement.Category).Append('|')
                .Append(achievement.Hidden ? '1' : '0').Append('|')
                .Append(achievement.IsCompleted ? '1' : '0').Append('|')
                .Append(achievement.Progress.ToString("R", CultureInfo.InvariantCulture))
                .Append('\n');
            foreach (BunnyAchievementCondition condition in achievement.Conditions)
            {
                output.Append("C|")
                    .Append(Hex(achievement.Name)).Append('|')
                    .Append(Hex(condition.Key)).Append('|')
                    .Append((int)condition.Kind).Append('|')
                    .Append(condition.Maximum.ToString("R", CultureInfo.InvariantCulture)).Append('|')
                    .Append(condition.Value.ToString("R", CultureInfo.InvariantCulture))
                    .Append('\n');
            }
        }
        if (output.Length > 512 * 1024)
            throw new InvalidOperationException("Metadata de conquistas excede 512 KiB");
        return output.ToString();
    }
}

public enum BunnyAchievementOperation
{
    Bind = 0,
    NpcKilled = 1,
    ItemPickup = 2,
    ItemCraft = 3,
    TileDestroyed = 4,
    Save = 5,
    Load = 6,
    Unload = 7,
    Metadata = 8,
    ConfigureStorage = 9,
    Flush = 10
}
