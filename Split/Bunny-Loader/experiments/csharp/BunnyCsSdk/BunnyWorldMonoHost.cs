using System;
using System.IO;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>
/// Ponto único de entrada para a thread proprietária do MonoVM nativo.
/// Não acessa Terraria, arquivos de mundo, QuickJS nem a thread gráfica.
/// O host deve chamar Confirm apenas após o sidecar atômico ser gravado.
/// </summary>
public static class BunnyWorldMonoHost
{
    private static BunnyWorldDispatcher _dispatcher = new();
    public static string LastError { get; private set; } = "";
    public static int RegisteredSystems => _dispatcher.RegisteredSystems;
    public static bool WorldActive => _dispatcher.WorldActive;

    /// <summary>
    /// Caminho deve vir do compilador nativo, não de um mod. Uma DLL só pode
    /// ser registrada antes de entrar no mundo. O registro não cria hooks.
    /// </summary>
    public static bool RegisterMod(string modUid, string assemblyPath)
    {
        LastError = "";
        if (_dispatcher.WorldActive || string.IsNullOrWhiteSpace(assemblyPath) ||
            assemblyPath.Length > 2048 || !Path.IsPathRooted(assemblyPath) ||
            !assemblyPath.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ||
            assemblyPath.Contains("..", StringComparison.Ordinal) ||
            !File.Exists(assemblyPath))
        {
            LastError = "Mod assembly path unavailable or world already active";
            return false;
        }
        try
        {
            Assembly assembly = Assembly.LoadFrom(assemblyPath);
            if (_dispatcher.TryRegisterAssembly(modUid, assembly, out var error))
                return true;
            LastError = error;
            return false;
        }
        catch (Exception ex)
        {
            LastError = "Mod world assembly rejected: " + ex.GetType().Name;
            return false;
        }
    }

    /// <summary>
    /// Base64 é usado somente nos raros eventos load/save. Bytes ausentes
    /// significam mundo novo, mas bytes inválidos não podem virar mundo novo.
    /// Epoch deve ser positivo, crescente, e vir do g_worldEpoch C++.
    /// </summary>
    public static bool Enter(long epoch, string? base64, bool authoritative)
    {
        LastError = "";
        if (epoch <= 0 || (base64 != null && base64.Length >
            4 * ((BunnyWorldDocument.MaxDocumentBytes + 2) / 3)))
        {
            LastError = "World payload or epoch invalid";
            return false;
        }
        byte[]? bytes = null;
        if (base64 != null)
        {
            try { bytes = Convert.FromBase64String(base64); }
            catch (FormatException)
            {
                LastError = "World payload Base64 malformed";
                return false;
            }
        }
        if (_dispatcher.TryEnterWorld((ulong)epoch, bytes, authoritative,
                                       out var error))
            return true;
        LastError = error;
        return false;
    }

    /// <summary>
    /// Retorno null significa falha e impede gravação. Nunca devolve um
    /// arquivo válido se um callback SaveWorldData falhou.
    /// </summary>
    public static string? Prepare(long epoch)
    {
        LastError = "";
        string error = "";
        if (epoch > 0 && _dispatcher.TryPrepareSave(
                (ulong)epoch, out byte[] snapshot, out error))
            return Convert.ToBase64String(snapshot);
        LastError = epoch <= 0 ? "Invalid world epoch" : error;
        return null;
    }

    public static bool Confirm(long epoch, string base64)
    {
        LastError = "";
        if (epoch <= 0 || base64 == null || base64.Length >
            4 * ((BunnyWorldDocument.MaxDocumentBytes + 2) / 3))
        {
            LastError = "Invalid world save acknowledgment";
            return false;
        }
        try
        {
            if (_dispatcher.TryConfirmSaved((ulong)epoch,
                Convert.FromBase64String(base64))) return true;
        }
        catch (FormatException) { }
        LastError = "World save confirmation rejected";
        return false;
    }

    public static bool Leave(long epoch)
    {
        LastError = "";
        string error = "";
        if (epoch > 0 && _dispatcher.TryLeaveWorld((ulong)epoch, out error))
            return true;
        LastError = epoch <= 0 ? "Invalid world epoch" : error;
        return false;
    }

    /// <summary>
    /// Hot-unload não é anunciado. Este método exige que nenhuma sessão
    /// esteja ativa; usado após descarregar o domínio gerenciado inteiro.
    /// </summary>
    public static bool ResetRegistrations()
    {
        if (_dispatcher.WorldActive)
        {
            LastError = "Cannot reset systems during an active world";
            return false;
        }
        _dispatcher = new BunnyWorldDispatcher();
        LastError = "";
        return true;
    }
}
