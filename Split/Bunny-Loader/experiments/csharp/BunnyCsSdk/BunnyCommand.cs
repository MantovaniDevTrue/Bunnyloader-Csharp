using System;
using System.Collections.Generic;
using System.Linq;

namespace BunnyLoader.Source;

[Flags]
public enum BunnyCommandType
{
    Chat = 1,
    Server = 2,
    Console = 4,
    World = 8
}

public static class BunnyCommandColors
{
    public const uint Response = 0xFFFFF014;
    public const uint Error = 0xFFFF1919;
    public const uint Success = 0xFF7CFC00;
}

public interface IBunnyCommandPlayer
{
    int Life { get; set; }
    int MaximumLife { get; }
    void HealEffect(int amount);
}

public interface IBunnyCommandWorld
{
    int NetMode { get; }
    bool DayTime { get; }
    double Time { get; }
    double DayLength { get; }
    double NightLength { get; }
    void SetClock(bool dayTime, double time);
    void BroadcastWorldData();
}

public sealed class BunnyCommandCaller
{
    private readonly Action<string, uint> reply;
    private readonly Func<string, string, string>? translate;

    public BunnyCommandType Type { get; }
    public IBunnyCommandPlayer? Player { get; }
    public IBunnyCommandWorld World { get; }

    public BunnyCommandCaller(
        BunnyCommandType type,
        IBunnyCommandPlayer? player,
        IBunnyCommandWorld world,
        Action<string, uint> reply,
        Func<string, string, string>? translate = null)
    {
        if (type == 0 || (type & ~(BunnyCommandType.Chat | BunnyCommandType.Server |
            BunnyCommandType.Console)) != 0)
            throw new ArgumentOutOfRangeException(nameof(type));
        Type = type;
        Player = player;
        World = world ?? throw new ArgumentNullException(nameof(world));
        this.reply = reply ?? throw new ArgumentNullException(nameof(reply));
        this.translate = translate;
    }

    public void Reply(string text, uint color = BunnyCommandColors.Response)
    {
        // O chat do Terraria recebe uma linha por mensagem. Não enviar texto
        // arbitrariamente grande nem permitir que uma resposta crie milhares de linhas.
        string[] lines = (text ?? string.Empty).Split('\n');
        for (int i = 0; i < Math.Min(lines.Length, 16); i++)
        {
            string line = lines[i].TrimEnd('\r');
            reply(line.Length > 512 ? line[..512] : line, color);
        }
    }

    public string Translate(string key, string fallback)
        => translate?.Invoke(key, fallback) ?? fallback;
}

public sealed class BunnyUsageException : Exception
{
    public uint Color { get; }

    public BunnyUsageException(string message = "", uint color = BunnyCommandColors.Error)
        : base(message)
    {
        Color = color;
    }
}

/// <summary>
/// Contrato C# de um comando de chat. O host cria o caller e chama o dispatcher
/// na origem correta; nenhum callback depende de JavaScript ou QuickJS.
/// </summary>
public abstract class BunnyCommand
{
    public abstract string Command { get; }
    public virtual BunnyCommandType Type => BunnyCommandType.Chat;
    public virtual IReadOnlyList<string> Aliases => Array.Empty<string>();
    public virtual string Usage => "/" + Command;
    public virtual string Description => "";
    public virtual bool IsCaseSensitive => false;
    public virtual void SetStaticDefaults() { }

    // false pede que o dispatcher responda com Usage; exceções de uso aceitam
    // uma mensagem localizada. Um erro inesperado não pode derrubar o chat.
    public abstract bool Action(BunnyCommandCaller caller, string input, IReadOnlyList<string> args);
}

public readonly record struct BunnyCommandHelp(string Command, string Description);

/// <summary>
/// Registro por mod, nomes alternativos, /mod:comando, permissões e /help.
/// Não contém estados estáticos: uma instância pertence a uma sessão do loader.
/// Usar no thread/worker dono da sessão e descartar em unload.
/// </summary>
public sealed class BunnyCommandDispatcher
{
    private sealed record Entry(string Owner, BunnyCommand Command, string[] Names);
    private readonly List<Entry> entries = new();
    private readonly Action<string>? diagnostics;

    public int Count => entries.Count;

    public BunnyCommandDispatcher(Action<string>? diagnostics = null)
    {
        this.diagnostics = diagnostics;
    }

    public void Register(string modId, BunnyCommand command)
    {
        ArgumentNullException.ThrowIfNull(command);
        string owner = Normalize(modId, false);
        if (owner.Length == 0)
            throw new ArgumentException("ID do mod inválido", nameof(modId));
        if (entries.Count >= 256)
            throw new InvalidOperationException("Limite de 256 comandos por sessão");

        var names = new List<string>();
        AddName(names, command.Command);
        if (command.Aliases.Count > 8)
            throw new ArgumentException("Máximo de oito aliases por comando");
        foreach (string name in command.Aliases)
            AddName(names, name);
        if (names.Count == 0)
            throw new ArgumentException("O comando precisa de um nome");

        if ((command.Type & ~(BunnyCommandType.Chat | BunnyCommandType.Server |
              BunnyCommandType.Console | BunnyCommandType.World)) != 0 ||
            command.Type == 0)
            throw new ArgumentException("Tipo de comando inválido");

        if (entries.Any(entry => entry.Owner == owner &&
            entry.Names.Any(name => names.Contains(name, StringComparer.Ordinal))))
            throw new InvalidOperationException("Nome ou alias duplicado dentro do mod " + owner);

        // Só publicar depois de SetStaticDefaults: se ele falhar, o registro
        // não fica parcialmente instalado e os outros mods continuam íntegros.
        command.SetStaticDefaults();
        entries.Add(new Entry(owner, command, names.ToArray()));
    }

    public void UnregisterMod(string modId)
        => entries.RemoveAll(entry => entry.Owner == Normalize(modId, false));

    public void Clear() => entries.Clear();

    public bool Handle(string? input, BunnyCommandCaller caller)
    {
        ArgumentNullException.ThrowIfNull(caller);
        if (string.IsNullOrWhiteSpace(input)) return false;
        string text = input.Trim();
        if (!text.StartsWith("/", StringComparison.Ordinal) || text.Length > 1024)
            return false;

        int space = text.IndexOfAny(new[] { ' ', '\t', '\r', '\n' });
        string token = (space < 0 ? text : text[..space])[1..];
        if (token.Length == 0) return false;
        string? owner = null;
        int colon = token.IndexOf(':');
        if (colon >= 0)
        {
            owner = Normalize(token[..colon], false);
            token = token[(colon + 1)..];
            if (owner.Length == 0) return false;
        }
        string name = Normalize(token, false);
        if (name.Length == 0) return false;

        Entry[] matches = entries.Where(e =>
            e.Names.Contains(name, StringComparer.Ordinal) &&
            Matches(e.Command.Type, caller.Type, caller.World.NetMode)).ToArray();
        if (matches.Length == 0) return false;

        if (owner != null)
        {
            Entry? chosen = Array.Find(matches, e => e.Owner == owner);
            if (chosen == null)
            {
                caller.Reply("O mod " + owner + " não tem o comando /" + name + ".", BunnyCommandColors.Error);
                return true;
            }
            matches = new[] { chosen };
        }
        else if (matches.Length > 1)
        {
            caller.Reply("O comando /" + name + " existe em mais de um mod. Use:", BunnyCommandColors.Error);
            foreach (Entry match in matches)
                caller.Reply("/" + match.Owner + ":" + name, BunnyCommandColors.Success);
            return true;
        }

        Entry selected = matches[0];
        string line = selected.Command.IsCaseSensitive ? text : text.ToLowerInvariant();
        string[] parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        string[] args = parts.Skip(1).Take(32).ToArray();
        if (parts.Length > 33)
        {
            caller.Reply("Uso: " + selected.Command.Usage, BunnyCommandColors.Error);
            return true;
        }

        try
        {
            if (!selected.Command.Action(caller, line, args))
                caller.Reply(selected.Command.Usage);
        }
        catch (BunnyUsageException e)
        {
            caller.Reply(string.IsNullOrEmpty(e.Message) ? "Uso: " + selected.Command.Usage : e.Message, e.Color);
        }
        catch (Exception e)
        {
            diagnostics?.Invoke(selected.Owner + ":" + name + ".Action: " + e);
            caller.Reply("Uso: " + selected.Command.Usage, BunnyCommandColors.Error);
        }
        return true;
    }

    public IReadOnlyList<BunnyCommandHelp> GetHelp(BunnyCommandType callerType, int netMode)
    {
        var help = new List<BunnyCommandHelp>();
        foreach (Entry entry in entries)
        {
            if (!Matches(entry.Command.Type, callerType, netMode)) continue;
            string name = entry.Names[0];
            bool shared = entries.Count(e => e.Names.Contains(name, StringComparer.Ordinal) &&
                Matches(e.Command.Type, callerType, netMode)) > 1;
            help.Add(new BunnyCommandHelp((shared ? entry.Owner + ":" : "") + name,
                entry.Command.Description));
        }
        return help;
    }

    public static bool Matches(BunnyCommandType commandType, BunnyCommandType callerType, int netMode)
    {
        // O Android usa netMode como bits: 0 solo, 1 cliente, 2 servidor,
        // 3 telefone que hospeda e joga. World vai para o lado autoritativo.
        if ((commandType & BunnyCommandType.World) != 0)
        {
            if ((netMode & 2) != 0) commandType |= BunnyCommandType.Server;
            else if (netMode == 0) commandType |= BunnyCommandType.Chat;
        }
        return (commandType & callerType) != 0;
    }

    private static void AddName(List<string> names, string raw)
    {
        string name = Normalize(raw, true);
        if (name.Length == 0 || !names.All(other => other != name))
            throw new ArgumentException("Nome ou alias inválido/duplicado: " + raw);
        names.Add(name);
    }

    private static string Normalize(string? raw, bool removeSlash)
    {
        if (raw is null) return "";
        string name = raw.Trim();
        if (removeSlash) name = name.TrimStart('/');
        if (name.Length is < 1 or > 64) return "";
        // IDs seguros para usar em /mod:comando e evitar caracteres de chat.
        foreach (char c in name)
            if (!(c is >= 'a' and <= 'z' or >= 'A' and <= 'Z' or >= '0' and <= '9' or '_' or '-'))
                return "";
        return name.ToLowerInvariant();
    }
}

/// <summary>Mesma aritmética do /addtime do ExampleModBunny, inclusive ticks negativos.</summary>
public static class BunnyCommandClock
{
    public static (bool IsDay, double Time) AddTicks(
        bool dayTime, double time, double dayLength, double nightLength, long ticks)
    {
        double cycle = dayLength + nightLength;
        if (!double.IsFinite(cycle) || !double.IsFinite(time) ||
            dayLength <= 0 || nightLength <= 0)
            throw new ArgumentOutOfRangeException(nameof(dayLength));
        double absolute = time + (dayTime ? 0 : dayLength) + ticks;
        double normalized = ((absolute % cycle) + cycle) % cycle;
        bool newDay = normalized < dayLength;
        return (newDay, newDay ? normalized : normalized - dayLength);
    }
}
