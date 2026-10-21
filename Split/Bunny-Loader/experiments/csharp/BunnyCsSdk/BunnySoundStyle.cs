using System;
using System.Globalization;
using System.Text;

namespace BunnyLoader.Source;

public enum BunnySoundLimitBehavior { IgnoreNew = 0, ReplaceOldest = 1 }

/// <summary>
/// Som de uso carregado pelo serviço nativo Android compartilhado com JavaScript.
/// O caminho é relativo a content/, com extensão explícita. Nenhuma VM JS é invocada.
/// </summary>
public sealed class BunnySoundStyle
{
    public string Path { get; }
    public float Volume { get; }
    public float Pitch { get; }
    public float PitchVariance { get; }
    public int MaxInstances { get; }
    public BunnySoundLimitBehavior LimitBehavior { get; }

    public BunnySoundStyle(string path, float volume = 1, float pitch = 0,
        float pitchVariance = 0, int maxInstances = 1,
        BunnySoundLimitBehavior limitBehavior = BunnySoundLimitBehavior.ReplaceOldest)
    {
        if (string.IsNullOrWhiteSpace(path) || path.Length > 180 || Encoding.UTF8.GetByteCount(path) > 180 || path[0] == '/' ||
            path.Contains('\\') || path.Contains(':') || path.Contains('|') ||
            Array.Exists(path.Split('/'), part => part.Length == 0 || part == "." || part == "..") ||
            Array.Exists(path.ToCharArray(), char.IsControl) ||
            !(path.EndsWith(".wav", StringComparison.OrdinalIgnoreCase) ||
              path.EndsWith(".ogg", StringComparison.OrdinalIgnoreCase) ||
              path.EndsWith(".mp3", StringComparison.OrdinalIgnoreCase)))
            throw new ArgumentException("[CSharp][ERROR][Sound] API=BunnySoundStyle Stage=ValidateAsset Expected=RelativeAudioPathWithExtension Reason=InvalidAssetPath", nameof(path));
        if (!float.IsFinite(volume) || volume < 0 || volume > 4)
            throw new ArgumentOutOfRangeException(nameof(volume));
        if (!float.IsFinite(pitch) || pitch < -1 || pitch > 1)
            throw new ArgumentOutOfRangeException(nameof(pitch));
        if (!float.IsFinite(pitchVariance) || pitchVariance < 0 || pitchVariance > 2)
            throw new ArgumentOutOfRangeException(nameof(pitchVariance));
        if (maxInstances < 0 || maxInstances > 256)
            throw new ArgumentOutOfRangeException(nameof(maxInstances));
        if (limitBehavior != BunnySoundLimitBehavior.IgnoreNew && limitBehavior != BunnySoundLimitBehavior.ReplaceOldest)
            throw new ArgumentOutOfRangeException(nameof(limitBehavior));
        Path = path; Volume = volume; Pitch = pitch; PitchVariance = pitchVariance;
        MaxInstances = maxInstances; LimitBehavior = limitBehavior;
    }

    /// <summary>Descritor de carga; o host ainda valida asset, tipo e método nativos.</summary>
    public string Definition => string.Join("\t", Convert.ToHexString(Encoding.UTF8.GetBytes(Path)),
        Volume.ToString("R", CultureInfo.InvariantCulture), Pitch.ToString("R", CultureInfo.InvariantCulture),
        PitchVariance.ToString("R", CultureInfo.InvariantCulture), MaxInstances.ToString(CultureInfo.InvariantCulture),
        ((int)LimitBehavior).ToString(CultureInfo.InvariantCulture));
}
