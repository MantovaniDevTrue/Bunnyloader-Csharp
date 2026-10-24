using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// Entrada exclusiva do worker MonoVM para o CommandLoader.
/// Nao acessa Terraria, IL2CPP nem JavaScript: recebe somente snapshots e
/// devolve linhas de resultado para a thread nativa do jogo consumir.
/// </summary>
public static class BunnyCommandWorker
{
    private static readonly object Gate = new();
    private static BunnyCommandRegistry registry = new();
    private static readonly Dictionary<string, BunnyModAssemblyLease> Leases = new(StringComparer.Ordinal);

    public const int MaxReplyLines = 16;
    public const int MaxReplyUtf8Bytes = 512;

    /// <summary>
    /// Chamado pelo host apenas depois da verificacao de hash do assembly.
    /// Os comandos permanecem no worker, com estado independente do mundo.
    /// </summary>
    public static int RegisterAssembly(string modId, string path)
    {
        lock (Gate)
        {
            if (Leases.ContainsKey(modId))
                throw new InvalidOperationException("ModCommand already registered: " + modId);
            var lease = BunnyModAssemblyRegistry.Acquire(modId, path);
            try
            {
                int count = registry.RegisterAssembly(modId, lease.Assembly);
                Leases.Add(modId, lease);
                return count;
            }
            catch
            {
                lease.Dispose();
                throw;
            }
        }
    }

    public static void Unregister(string modId)
    {
        if (string.IsNullOrWhiteSpace(modId)) return;
        lock (Gate)
        {
            registry.Unregister(modId);
            if (Leases.Remove(modId, out BunnyModAssemblyLease? lease))
                lease.Dispose();
        }
    }

    public static void Clear()
    {
        lock (Gate)
        {
            registry = new BunnyCommandRegistry();
            foreach (BunnyModAssemblyLease lease in Leases.Values) lease.Dispose();
            Leases.Clear();
        }
    }

    /// <summary>
    /// Formato deterministico, com texto UTF-8 em hexadecimal:
    /// BUNNY_COMMAND_RESULT_V1, HANDLED 0/1, seguido por REPLY R G B HEX.
    /// Nenhuma resposta pode designar outro jogador: o C++ guarda o destino
    /// original no ticket e aplica todas as mensagens no game thread.
    /// </summary>
    public static string Execute(string input, int playerIndex, int netMode, int callerKind)
    {
        var responses = new List<BunnyCommandReply>();
        BunnyCommandAction? action = null;
        var caller = new BunnyCommandCaller(playerIndex, netMode,
            (BunnyCommandCallerKind)callerKind, reply =>
            {
                if (responses.Count >= MaxReplyLines) return;
                byte[] bytes = Encoding.UTF8.GetBytes(reply.Text);
                if (bytes.Length <= MaxReplyUtf8Bytes)
                    responses.Add(reply);
                else
                {
                    // Cortar por bytes preservando limites Unicode evita texto
                    // invalido ao atravessar a ponte MonoVM -> IL2CPP.
                    int count = MaxReplyUtf8Bytes;
                    while (count > 0 && (bytes[count] & 0xC0) == 0x80) --count;
                    responses.Add(reply with { Text = Encoding.UTF8.GetString(bytes, 0, count) });
                }
            }, requested =>
            {
                if (action.HasValue || responses.Count != 0)
                    throw new InvalidOperationException("Uma acao nao pode compartilhar resposta antecipada");
                action = requested;
            });
        bool handled;
        lock (Gate) handled = registry.TryHandle(input, caller);
        var output = new StringBuilder(64 + responses.Count * 128);
        output.Append("BUNNY_COMMAND_RESULT_V1\nHANDLED\t").Append(handled ? '1' : '0').Append('\n');
        // A confirmacao de cura somente pode ser enviada pelo C++ depois
        // de alterar statLife. Rejeitar resposta antecipada de outros comandos.
        if (action.HasValue && responses.Count != 0)
        {
            action = null;
            responses.Clear();
            responses.Add(new BunnyCommandReply("Comando: efeito invalido", 255, 25, 25));
        }
        if (handled && action.HasValue)
            output.Append("ACTION\t").Append(action.Value.Kind == BunnyCommandActionKind.Heal
                ? "HEAL" : "ADDTIME").Append('\t')
                .Append(action.Value.Amount).Append('\n');
        foreach (var reply in responses)
        {
            output.Append("REPLY\t").Append(reply.Red).Append('\t').Append(reply.Green)
                .Append('\t').Append(reply.Blue).Append('\t')
                .Append(Convert.ToHexString(Encoding.UTF8.GetBytes(reply.Text))).Append('\n');
        }
        return output.ToString();
    }
}
