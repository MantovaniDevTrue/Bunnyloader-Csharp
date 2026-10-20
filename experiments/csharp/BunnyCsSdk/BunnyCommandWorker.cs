using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
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

    public const int MaxReplyLines = 16;
    public const int MaxReplyUtf8Bytes = 512;

    /// <summary>
    /// Chamado pelo host apenas depois da verificacao de hash do assembly.
    /// Os comandos permanecem no worker, com estado independente do mundo.
    /// </summary>
    public static int RegisterAssembly(string modId, string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !Path.IsPathFullyQualified(path) ||
            !File.Exists(path) || new FileInfo(path).Length > 32 * 1024 * 1024)
            throw new ArgumentException("Assembly de ModCommand invalido", nameof(path));
        var assembly = Assembly.Load(File.ReadAllBytes(path));
        lock (Gate) return registry.RegisterAssembly(modId, assembly);
    }

    public static void Unregister(string modId)
    {
        lock (Gate) registry.Unregister(modId);
    }

    public static void Clear()
    {
        lock (Gate) registry = new BunnyCommandRegistry();
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
            });
        bool handled;
        lock (Gate) handled = registry.TryHandle(input, caller);
        var output = new StringBuilder(64 + responses.Count * 128);
        output.Append("BUNNY_COMMAND_RESULT_V1\nHANDLED\t").Append(handled ? '1' : '0').Append('\n');
        foreach (var reply in responses)
        {
            output.Append("REPLY\t").Append(reply.Red).Append('\t').Append(reply.Green)
                .Append('\t').Append(reply.Blue).Append('\t')
                .Append(Convert.ToHexString(Encoding.UTF8.GetBytes(reply.Text))).Append('\n');
        }
        return output.ToString();
    }
}
