using System;
using System.IO;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// Salva o BACH1 de um mod na área interna do Bunny Loader.
/// Arquivo temporário e rename impedem que uma gravação parcial substitua
/// o último progresso válido. Nunca lê ou altera o save vanilla do Terraria.
/// Chamado apenas no worker MonoVM serializado.
/// </summary>
public sealed class BunnyAchievementStore
{
    private const int MaxBytes = 1024 * 1024;
    private const int AutosaveSeconds = 3;
    private readonly BunnyAchievementRegistry registry;
    private string? filename;
    private long persistedVersion;
    private DateTime lastWrite = DateTime.MinValue;
    private bool configured;

    public BunnyAchievementStore(BunnyAchievementRegistry registry)
        => this.registry = registry ?? throw new ArgumentNullException(nameof(registry));

    private string? configuredModUid;
    public bool IsConfigured => configured;
    public string Filename => filename ?? "";

    public void Configure(string directory, string modUid)
    {
        if (configured)
            throw new InvalidOperationException("Achievement storage already configured");
        if (string.IsNullOrWhiteSpace(directory) ||
            !Path.IsPathFullyQualified(directory) ||
            directory.Length > 2048 || !Directory.Exists(directory))
            throw new ArgumentException("Achievement storage directory must exist", nameof(directory));
        if (string.IsNullOrWhiteSpace(modUid) || modUid.Length > 96)
            throw new ArgumentException("Invalid achievement mod UID", nameof(modUid));
        foreach (char ch in modUid)
        {
            if (!(ch is >= 'a' and <= 'z' or >= 'A' and <= 'Z' or >= '0' and <= '9' or '_' or '-' or '.'))
                throw new ArgumentException("Invalid achievement mod UID", nameof(modUid));
        }
        string path = Path.Combine(Path.GetFullPath(directory), modUid + ".bach");
        // Read and validate before changing state: corrupt saves never become
        // an empty registry, and are never overwritten during initialization.
        if (File.Exists(path))
        {
            var info = new FileInfo(path);
            if (info.Length < 1 || info.Length > MaxBytes)
                throw new InvalidDataException("Achievement save size invalid");
            var strictUtf8 = new UTF8Encoding(false, true);
            string saved = strictUtf8.GetString(File.ReadAllBytes(path));
            registry.Load(saved);
        }
        filename = path;
        configuredModUid = modUid;
        persistedVersion = registry.ProgressChanges;
        lastWrite = DateTime.UtcNow;
        configured = true;
    }

    public void SwitchProfile(string directory, string modUid)
    {
        if (!configured || filename == null || configuredModUid != modUid)
            throw new InvalidOperationException("Achievement profile not configured for this mod");
        if (string.IsNullOrWhiteSpace(directory) ||
            !Path.IsPathFullyQualified(directory) ||
            directory.Length > 2048 || !Directory.Exists(directory))
            throw new ArgumentException("Achievement profile directory must exist", nameof(directory));

        string destination = Path.Combine(Path.GetFullPath(directory), modUid + ".bach");
        if (string.Equals(filename, destination, StringComparison.Ordinal))
            return;

        string? incoming = null;
        if (File.Exists(destination))
        {
            var info = new FileInfo(destination);
            if (info.Length < 1 || info.Length > MaxBytes)
                throw new InvalidDataException("Achievement profile size invalid");
            incoming = new UTF8Encoding(false, true).GetString(File.ReadAllBytes(destination));
        }

        // Worker FIFO serializes the transition. A failed restore must leave
        // the current character and its save path untouched.
        string previous = registry.Save();
        try
        {
            registry.Load(incoming);
        }
        catch
        {
            registry.Load(previous);
            throw;
        }

        try
        {
            // Save the OLD profile using its original path and dirty revision.
            // Do not write anything to the new profile before committing.
            if (persistedVersion != registry.ProgressChanges)
            {
                registry.Load(previous);
                Flush();
                registry.Load(incoming);
            }
        }
        catch
        {
            registry.Load(previous);
            throw;
        }

        filename = destination;
        persistedVersion = registry.ProgressChanges;
        lastWrite = DateTime.UtcNow;
    }

    public bool Flush(bool force = false)
    {
        if (!configured || filename == null)
            throw new InvalidOperationException("Achievement storage not configured");
        if (!force && persistedVersion == registry.ProgressChanges)
            return false;

        string data = registry.Save();
        byte[] bytes = new UTF8Encoding(false, true).GetBytes(data);
        if (bytes.Length > MaxBytes)
            throw new InvalidDataException("Achievement save exceeds 1 MiB");

        // Do not delete the old save until the new snapshot is durable.
        string temporary = filename + ".tmp";
        try
        {
            using (var stream = new FileStream(temporary, FileMode.Create,
                       FileAccess.Write, FileShare.None, 8192))
            {
                stream.Write(bytes);
                stream.Flush(flushToDisk: true);
            }
            File.Move(temporary, filename, overwrite: true);
            persistedVersion = registry.ProgressChanges;
            lastWrite = DateTime.UtcNow;
            return true;
        }
        catch
        {
            try { File.Delete(temporary); }
            catch (IOException) { }
            throw;
        }
    }

    public void Autosave(bool justCompleted)
    {
        if (!configured || persistedVersion == registry.ProgressChanges) return;
        if (justCompleted ||
            DateTime.UtcNow - lastWrite >= TimeSpan.FromSeconds(AutosaveSeconds))
            Flush();
    }

    public void MarkExternalRestore() => persistedVersion = -1;
}
