using System;
using System.Collections.Generic;
using System.Linq;

namespace BunnyLoader.Source;

/// <summary>Onde um comando pode ser executado. World usa o mundo local ou o servidor autoritativo.</summary>
[Flags]
public enum BunnyCommandType
{
    Chat = 1,
    Server = 2,
    World = 4
}

public enum BunnyCommandCallerKind
{
    Chat = 1,
    Server = 2
}

/// <summary>Resposta sem referencias IL2CPP: somente texto e RGB atravessam a ponte nativa.</summary>
public readonly record struct BunnyCommandReply(string Text, byte Red = 255, byte Green = 240, byte Blue = 20);

public sealed class BunnyUsageException : Exception
{
    public BunnyUsageException(string message = "") : base(message) { }
}

/// <summary>
/// Snapshot de quem digitou o comando. O host fornece a resposta na thread correta;
/// um comando C# nao deve acessar objetos vivos de Terraria pelo worker Mono.
/// </summary>
public sealed class BunnyCommandCaller
{
    private readonly Action<BunnyCommandReply> reply;

    public int PlayerIndex { get; }
    public int NetworkMode { get; }
    public BunnyCommandCallerKind Kind { get; }

    public BunnyCommandCaller(int playerIndex, int networkMode, BunnyCommandCallerKind kind,
        Action<BunnyCommandReply> reply)
    {
        if (playerIndex < -1 || playerIndex > 255)
            throw new ArgumentOutOfRangeException(nameof(playerIndex));
        if (networkMode < 0 || networkMode > 3)
            throw new ArgumentOutOfRangeException(nameof(networkMode));
        if (kind != BunnyCommandCallerKind.Chat && kind != BunnyCommandCallerKind.Server)
            throw new ArgumentOutOfRangeException(nameof(kind));
        this.reply = reply ?? throw new ArgumentNullException(nameof(reply));
        PlayerIndex = playerIndex;
        NetworkMode = networkMode;
        Kind = kind;
    }

    public void Reply(string message, byte red = 255, byte green = 240, byte blue = 20)
    {
        if (message == null) throw new ArgumentNullException(nameof(message));
        foreach (string line in message.Replace("\r\n", "\n", StringComparison.Ordinal).Split('\n'))
            reply(new BunnyCommandReply(line, red, green, blue));
    }
}

public sealed class BunnyCommandContext
{
    public BunnyCommandCaller Caller { get; }
    public string Input { get; }
    public IReadOnlyList<string> Arguments { get; }

    internal BunnyCommandContext(BunnyCommandCaller caller, string input, string[] arguments)
    {
        Caller = caller;
        Input = input;
        Arguments = Array.AsReadOnly(arguments);
    }
}

/// <summary>
/// Um comando por classe exportada. A chamada Action ocorre no MonoVM, nunca na thread grafica.
/// O host deve registrar as classes e encaminhar mensagens somente quando o hook nativo estiver ativo.
/// </summary>
public abstract class BunnyCommand
{
    public abstract string Command { get; }
    public virtual IReadOnlyList<string> Aliases => Array.Empty<string>();
    public virtual BunnyCommandType Type => BunnyCommandType.Chat;
    public virtual string Usage => "/" + Command;
    public virtual string Description => "";
    public virtual bool IsCaseSensitive => false;
    public abstract void Action(BunnyCommandContext context);
}

/// <summary>
/// Registro por mod, sem estado global entre mundos. Nao instala hooks nem executa Terraria sozinho.
/// O snapshot de nomes e protegido; callbacks sao chamados fora do lock.
/// </summary>
public sealed class BunnyCommandRegistry
{
    public const int MaxCommandsPerMod = 128;
    public const int MaxAliasesPerCommand = 16;
    public const int MaxInputLength = 1024;

    private sealed record Entry(string Owner, BunnyCommand Command, string CanonicalName);
    private readonly object gate = new();
    private readonly Dictionary<string, List<Entry>> names = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, List<Entry>> owners = new(StringComparer.OrdinalIgnoreCase);
    private readonly Action<Exception>? onError;

    public BunnyCommandRegistry(Action<Exception>? onError = null) => this.onError = onError;

    /// <summary>
    /// O worker chama isto uma vez apos carregar a montagem do mod. Ordem por FullName
    /// garante IDs e diagnosticos deterministas; falhas nao deixam registros parciais.
    /// </summary>
    public int RegisterAssembly(string modId, System.Reflection.Assembly assembly)
    {
        ArgumentNullException.ThrowIfNull(assembly);
        Type[] types = assembly.GetTypes()
            .Where(t => t.IsClass && !t.IsAbstract && !t.ContainsGenericParameters &&
                        typeof(BunnyCommand).IsAssignableFrom(t))
            .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();
        return RegisterTypes(modId, types);
    }

    public int RegisterTypes(string modId, IEnumerable<Type> types)
    {
        ArgumentNullException.ThrowIfNull(types);
        Type[] snapshot = types.Take(MaxCommandsPerMod + 1).ToArray();
        if (snapshot.Length == 0 || snapshot.Length > MaxCommandsPerMod)
            throw new ArgumentOutOfRangeException(nameof(types));
        var instances = new List<BunnyCommand>(snapshot.Length);
        foreach (Type type in snapshot)
        {
            if (type == null || !type.IsClass || type.IsAbstract || type.ContainsGenericParameters ||
                !typeof(BunnyCommand).IsAssignableFrom(type) ||
                type.GetConstructor(Type.EmptyTypes) == null)
                throw new ArgumentException("Classe precisa estender BunnyCommand e ter construtor publico vazio",
                    nameof(types));
            if (Activator.CreateInstance(type) is not BunnyCommand instance)
                throw new InvalidOperationException("Falha ao criar comando " + type.FullName);
            instances.Add(instance);
        }
        return Register(modId, instances);
    }

    public int Register(string modId, IEnumerable<BunnyCommand> commands)
    {
        CheckName(modId, nameof(modId));
        ArgumentNullException.ThrowIfNull(commands);
        var commandList = commands.ToArray();
        if (commandList.Length == 0 || commandList.Length > MaxCommandsPerMod)
            throw new ArgumentOutOfRangeException(nameof(commands));
        var proposed = new List<(string Name, Entry Entry)>();
        var unique = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (BunnyCommand command in commandList)
        {
            if (command == null) throw new ArgumentException("Classe de comando nula", nameof(commands));
            string name = Normalize(command.Command);
            if (command.Type == 0 || (command.Type & ~(BunnyCommandType.Chat | BunnyCommandType.Server | BunnyCommandType.World)) != 0)
                throw new ArgumentException("Tipo de comando invalido", nameof(commands));
            var entry = new Entry(modId, command, name);
            var aliases = command.Aliases ?? throw new ArgumentException("Aliases nulos", nameof(commands));
            if (aliases.Count > MaxAliasesPerCommand)
                throw new ArgumentOutOfRangeException(nameof(commands), "Aliases excedem o limite");
            proposed.Add((name, entry));
            foreach (string alias in aliases)
                proposed.Add((Normalize(alias), entry));
            if (proposed.Count > MaxCommandsPerMod * (1 + MaxAliasesPerCommand))
                throw new ArgumentOutOfRangeException(nameof(commands));
        }

        foreach (var pair in proposed)
            if (!unique.Add(pair.Name))
                throw new ArgumentException("Nome ou alias duplicado no mod: " + pair.Name, nameof(commands));

        lock (gate)
        {
            if (owners.ContainsKey(modId))
                throw new InvalidOperationException("Mod ja registrou comandos: " + modId);
            var stored = new List<Entry>(commandList.Length);
            foreach (var pair in proposed)
            {
                if (!names.TryGetValue(pair.Name, out var entries))
                    names[pair.Name] = entries = new List<Entry>();
                entries.Add(pair.Entry);
                if (!stored.Contains(pair.Entry)) stored.Add(pair.Entry);
            }
            owners.Add(modId, stored);
        }
        return commandList.Length;
    }

    public void Unregister(string modId)
    {
        if (string.IsNullOrWhiteSpace(modId)) return;
        lock (gate)
        {
            if (!owners.Remove(modId, out var entries)) return;
            var owned = new HashSet<Entry>(entries);
            foreach (string key in names.Keys.ToArray())
            {
                names[key].RemoveAll(e => owned.Contains(e));
                if (names[key].Count == 0) names.Remove(key);
            }
        }
    }

    public bool TryHandle(string? input, BunnyCommandCaller caller)
    {
        ArgumentNullException.ThrowIfNull(caller);
        if (string.IsNullOrWhiteSpace(input) || input.Length > MaxInputLength) return false;
        string text = input.Trim();
        if (text.Length < 2 || text[0] != '/') return false;
        int split = 1;
        while (split < text.Length && !char.IsWhiteSpace(text[split])) split++;
        string token = text.Substring(1, split - 1);
        int separator = token.IndexOf(':');
        string? owner = separator >= 0 ? token.Substring(0, separator) : null;
        string name = separator >= 0 ? token[(separator + 1)..] : token;
        if (!IsValidName(name) || (owner != null && !IsValidName(owner))) return false;

        List<Entry> available;
        lock (gate)
            available = names.TryGetValue(name, out var entries)
                ? entries.Where(x => Matches(x.Command.Type, caller)).ToList()
                : new List<Entry>();
        if (available.Count == 0) return false;

        Entry? selected = null;
        if (owner != null)
        {
            selected = available.FirstOrDefault(x => string.Equals(x.Owner, owner, StringComparison.OrdinalIgnoreCase));
            if (selected == null)
            {
                caller.Reply("O mod " + owner + " nao tem o comando /" + name + ".", 255, 25, 25);
                return true;
            }
        }
        else if (available.Count == 1) selected = available[0];
        else
        {
            caller.Reply("O comando /" + name + " existe em mais de um mod. Use:", 255, 25, 25);
            foreach (Entry entry in available)
                caller.Reply("/" + entry.Owner + ":" + name, 124, 252, 0);
            return true;
        }

        // Mantem a mesma separacao por espacos do ExampleModBunny, sem parser de aspas inventado.
        string line = selected!.Command.IsCaseSensitive ? text : text.ToLowerInvariant();
        string[] parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        string[] arguments = parts.Skip(1).ToArray();
        try
        {
            selected.Command.Action(new BunnyCommandContext(caller, line, arguments));
        }
        catch (BunnyUsageException exception)
        {
            caller.Reply(string.IsNullOrEmpty(exception.Message) ? "Uso: " + selected.Command.Usage : exception.Message,
                255, 25, 25);
        }
        catch (Exception exception)
        {
            onError?.Invoke(exception);
            caller.Reply("Uso: " + selected.Command.Usage, 255, 25, 25);
        }
        return true;
    }

    public IReadOnlyList<(string Command, string Description)> GetHelp(BunnyCommandCaller caller)
    {
        ArgumentNullException.ThrowIfNull(caller);
        lock (gate)
        {
            return owners.Values.SelectMany(x => x)
                .Where(x => Matches(x.Command.Type, caller))
                .Select(x => (Command: names.TryGetValue(x.CanonicalName, out var collisions) && collisions.Count > 1
                    ? x.Owner + ":" + x.CanonicalName : x.CanonicalName,
                    Description: x.Command.Description))
                .OrderBy(x => x.Command, StringComparer.OrdinalIgnoreCase).ToArray();
        }
    }

    private static bool Matches(BunnyCommandType type, BunnyCommandCaller caller)
    {
        if ((type & BunnyCommandType.World) != 0)
            type |= (caller.NetworkMode & 2) != 0 ? BunnyCommandType.Server
                : caller.NetworkMode == 0 ? BunnyCommandType.Chat : 0;
        return (type & (caller.Kind == BunnyCommandCallerKind.Chat
            ? BunnyCommandType.Chat : BunnyCommandType.Server)) != 0;
    }

    private static string Normalize(string name)
    {
        if (name == null) throw new ArgumentNullException(nameof(name));
        string result = name.Trim().TrimStart('/');
        CheckName(result, nameof(name));
        return result.ToLowerInvariant();
    }

    private static void CheckName(string name, string parameter)
    {
        if (!IsValidName(name))
            throw new ArgumentException("Nome deve ter 1..64 caracteres ASCII (letras, numeros, _, - ou .)", parameter);
    }

    private static bool IsValidName(string? name)
    {
        if (string.IsNullOrEmpty(name) || name.Length > 64) return false;
        foreach (char c in name)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
                return false;
        return true;
    }
}
