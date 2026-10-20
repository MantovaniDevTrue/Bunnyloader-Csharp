using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

/// <summary>
/// SoundLoader C# independente. O cadastro e a reprodução entram numa fila limitada;
/// apenas a thread do Terraria instala o roteador e lê os campos IL2CPP da câmera.
/// </summary>
public static class BunnySounds
{
    /// <summary>
    /// Registra o arquivo de content/ do próprio mod. Repetir o mesmo estilo reutiliza
    /// a identidade nativa; o status pode continuar Pending até o SoundPool decodificar.
    /// </summary>
    public static BunnySoundClip Register(string modId, BunnySoundStyle style)
    {
        if (string.IsNullOrWhiteSpace(modId) || modId.Length > 128)
            throw new ArgumentException("Informe o uid real do mod.", nameof(modId));
        ArgumentNullException.ThrowIfNull(style);
        int id = NativeRegister(modId, style.Path, style.Volume, style.Pitch,
            style.PitchVariance, style.MaxInstances, (int)style.LimitBehavior);
        if (id <= 0)
            throw new InvalidOperationException("[CSharp][SoundLoader] Registro recusado: mod, arquivo ou limite inválido.");
        return new BunnySoundClip(modId, id);
    }

    /// <summary>
    /// Libera os estilos pertencentes ao uid informado, sem invalidar áudio de outros mods.
    /// O host libera os recursos na thread do jogo no próximo pump.
    /// </summary>
    public static void UnloadMod(string modId)
    {
        if (string.IsNullOrWhiteSpace(modId)) throw new ArgumentException(nameof(modId));
        NativeUnload(modId);
    }

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_register", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeRegister(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string owner,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        float volume, float pitch, float variance, int maxInstances, int behavior);

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_unload", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeUnload([MarshalAs(UnmanagedType.LPUTF8Str)] string owner);

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_state", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int State(int handle);

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_play", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Play(int handle, int x, int y, float volumeScale, float pitchOffset);

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_playback", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Playback(int handle, int request);

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_find_active", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int FindActive(int handle);

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_stop", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Stop(int handle, int request);

    [DllImport("bunny", EntryPoint = "bunny_csharp_sound_duration", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Duration(int handle);
}

/// <summary>0 = pendente, 1 = pronto, -1 = falhou, -2 = liberado/inexistente.</summary>
public readonly struct BunnySoundClip
{
    public string ModId { get; }
    public int Handle { get; }
    internal BunnySoundClip(string modId, int handle) { ModId = modId; Handle = handle; }
    public int State => BunnySounds.State(Handle);
    public int DurationMilliseconds => BunnySounds.Duration(Handle);

    /// <summary>
    /// -1/-1: som de interface sem posição. Coordenadas são pixels do mundo.
    /// Retorna um pedido assíncrono: Playback.Token fica disponível após o pump.
    /// Não faz reflexão IL2CPP nem chamada à Unity na thread MonoVM.
    /// </summary>
    public BunnySoundPlayback Play(int x = -1, int y = -1, float volumeScale = 1f,
        float pitchOffset = 0f)
    {
        if ((x == -1) != (y == -1)) throw new ArgumentException("Forneça x e y juntos.");
        if (!float.IsFinite(volumeScale) || volumeScale < 0f || volumeScale > 4f)
            throw new ArgumentOutOfRangeException(nameof(volumeScale));
        if (!float.IsFinite(pitchOffset) || pitchOffset < -1f || pitchOffset > 1f)
            throw new ArgumentOutOfRangeException(nameof(pitchOffset));
        int request = BunnySounds.Play(Handle, x, y, volumeScale, pitchOffset);
        return new BunnySoundPlayback(Handle, request);
    }

    /// <summary>Última instância ainda ativa, ou 0. Atualizado pelo pump nativo.</summary>
    public int FindActive() => BunnySounds.FindActive(Handle);
}

/// <summary>
/// Pedido de reprodução: 0 enquanto pendente; token positivo quando aceito;
/// -1 em falha ou cancelamento. Stop() também cancela um pedido ainda na fila.
/// </summary>
public readonly struct BunnySoundPlayback
{
    public int ClipHandle { get; }
    public int Request { get; }
    internal BunnySoundPlayback(int clipHandle, int request)
    { ClipHandle = clipHandle; Request = request; }
    public int Token => Request > 0 ? BunnySounds.Playback(ClipHandle, Request) : -1;
    public bool Stop() => Request > 0 && BunnySounds.Stop(ClipHandle, Request) != 0;
}
