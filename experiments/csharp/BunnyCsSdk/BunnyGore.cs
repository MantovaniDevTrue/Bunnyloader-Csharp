using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

/// <summary>
/// Os cinco campos de GoreID.Sets e ChildSafety.SafeGore da API nativa.
/// DisappearSpeed e DisappearSpeedAlpha usam 1 como no ExampleModBunny.
/// </summary>
public readonly struct BunnyGoreSettings
{
    public int SpecialAI { get; }
    public int DisappearSpeed { get; }
    public int DisappearSpeedAlpha { get; }
    public bool IsDrip { get; }
    public bool SafeGore { get; }

    public BunnyGoreSettings(int specialAI = 0, int disappearSpeed = 1,
                             int disappearSpeedAlpha = 1, bool isDrip = false,
                             bool safeGore = false)
    {
        if ((uint)specialAI > 255 || (uint)disappearSpeed > 255 ||
            (uint)disappearSpeedAlpha > 255)
            throw new ArgumentOutOfRangeException(nameof(specialAI),
                "Valores nativos GoreID devem estar entre 0 e 255.");
        SpecialAI = specialAI;
        DisappearSpeed = disappearSpeed;
        DisappearSpeedAlpha = disappearSpeedAlpha;
        IsDrip = isDrip;
        SafeGore = safeGore;
    }
}

/// <summary>
/// GoreLoader nativo C# para Terraria Android. Os PNGs de gore ficam na pasta
/// Content/Gores/ (ou qualquer subpasta Gores/ do pacote) e sao registrados
/// uma unica vez durante o carregamento do mod, sem QuickJS.
/// </summary>
public static class BunnyGore
{
    /// <summary>Última indisponibilidade da ponte, útil em testes sem Terraria.</summary>
    public static string LastHostError { get; private set; } = "";
    /// <summary>
    /// Retorna o ID nativo registrado para um PNG por nome sem .png, ou zero
    /// quando o nome nao pertence ao mod. Dois mods podem usar o mesmo nome
    /// porque a chave tambem inclui o id do manifest do pacote.
    /// </summary>
    public static int GetType(string modId, string name)
    {
        Validate(modId, name);
        return NativeType(modId, name);
    }

    /// <summary>
    /// Equivalente a ModGore.getTypeByName sem mod na pilha do ExampleModBunny.
    /// Quando dois mods usam o mesmo nome, retorna o primeiro registrado.
    /// Para evitar ambiguidades entre mods, prefira GetType(modId, name).
    /// </summary>
    public static int GetTypeAny(string name)
    {
        Validate("gorelookup", name);
        return NativeTypeAny(name);
    }

    /// <summary>
    /// Enfileira um gore com posicao/velocidade em pixels do mundo.
    /// A funcao nativa chama Terraria.Gore.NewGore na thread do jogo;
    /// true significa que o pedido foi aceito na fila, nao que ele ja
    /// foi desenhado. Nao chama IL2CPP diretamente no worker MonoVM.
    /// Nao aceita pedidos fora do mundo, com fila cheia ou sem ponte nativa.
    /// A falta do efeito opcional nunca interrompe OnKill; consulte LastHostError.
    /// </summary>
    public static bool TrySpawn(string modId, string name, float x, float y,
                                float velocityX, float velocityY, float scale = 1f)
    {
        Validate(modId, name);
        if (!float.IsFinite(x) || !float.IsFinite(y) ||
            !float.IsFinite(velocityX) || !float.IsFinite(velocityY) ||
            !float.IsFinite(scale) || scale < 0.1f || scale > 4f)
            return false;

        try
        {
            bool accepted = NativeSpawn(modId, name, x, y, velocityX, velocityY, scale) == 1;
            LastHostError = accepted ? "" : "NativeRequestRejected";
            return accepted;
        }
        catch (DllNotFoundException) { LastHostError = "LibBunnyUnavailable"; return false; }
        catch (EntryPointNotFoundException) { LastHostError = "GoreAbiUnavailable"; return false; }
    }

    /// <summary>
    /// Atualiza a IA especial, a velocidade de desaparecimento e as flags
    /// nativas IsDrip/SafeGore do tipo pertencente ao mod. A configuracao
    /// entra em uma fila limitada e e aplicada na thread do Terraria, inclusive
    /// antes de abrir o mundo. Os valores persistem apos recriar arrays.
    /// </summary>
    public static bool TryConfigure(string modId, string name,
                                    BunnyGoreSettings settings)
    {
        Validate(modId, name);
        try
        {
            bool accepted = NativeConfigure(modId, name, settings.SpecialAI,
                settings.DisappearSpeed, settings.DisappearSpeedAlpha,
                settings.IsDrip ? 1 : 0, settings.SafeGore ? 1 : 0) == 1;
            LastHostError = accepted ? "" : "NativeRequestRejected";
            return accepted;
        }
        catch (DllNotFoundException) { LastHostError = "LibBunnyUnavailable"; return false; }
        catch (EntryPointNotFoundException) { LastHostError = "GoreAbiUnavailable"; return false; }
    }

    private static void Validate(string modId, string name)
    {
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 128 ||
            modId.Contains('/') || modId.Contains('\\') ||
            modId.Contains('\r') || modId.Contains('\n'))
            throw new ArgumentException("O id deve ser o id do manifest do mod.", nameof(modId));

        if (string.IsNullOrWhiteSpace(name) || name.Length > 180 ||
            name.Contains('/') || name.Contains('\\') ||
            name.Contains('\r') || name.Contains('\n'))
            throw new ArgumentException("Use somente o nome do PNG sem extensao.", nameof(name));
    }

    [DllImport("bunny", EntryPoint = "bunny_csharp_gore_type",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeType(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

    [DllImport("bunny", EntryPoint = "bunny_csharp_gore_type_any",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeTypeAny(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

    [DllImport("bunny", EntryPoint = "bunny_csharp_gore_configure",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeConfigure(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name,
        int specialAI, int disappearSpeed, int disappearSpeedAlpha,
        int isDrip, int safeGore);

    [DllImport("bunny", EntryPoint = "bunny_csharp_gore_spawn",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSpawn(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modId,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name,
        float x, float y, float velocityX, float velocityY, float scale);
}
