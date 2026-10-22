using System;
using System.IO;
using System.Reflection;
using BunnyLoader.Source;

namespace BunnyLoader.SourceCompiler;

/// <summary>
/// Ponte preparada para o worker MonoVM: depois que SourceLoader reservar os
/// IDs globais, esta função executa SetStaticDefaults no Mono e escreve o
/// manifesto nativo. Não ativa HairLoader nem publica assets no Terraria.
/// </summary>
public static class CompiledHairProbe
{
    public static HairManifestSession Export(
        Assembly compiledMod, string modUid, string modRoot,
        string destination, int reservedFirstHairType)
    {
        if (compiledMod is null) throw new ArgumentNullException(nameof(compiledMod));
        if (string.IsNullOrWhiteSpace(modRoot))
            throw new ArgumentException("Diretório do mod não informado.", nameof(modRoot));
        if (string.IsNullOrWhiteSpace(destination))
            throw new ArgumentException("Destino do manifesto não informado.", nameof(destination));

        string root = Path.GetFullPath(modRoot).TrimEnd(
            Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        if (root.Length == 0 || !Directory.Exists(root) || new DirectoryInfo(root).LinkTarget is not null)
            throw new InvalidOperationException("Diretório raiz do mod inexistente ou simbólico.");

        string target = Path.GetFullPath(destination);
        string? folderPath = Path.GetDirectoryName(target);
        if (folderPath is null || !Directory.Exists(folderPath))
            throw new InvalidOperationException("Diretório do manifesto não existe.");
        // Um manifesto antigo não pode disfarçar a falha desta compilação.
        if (File.Exists(target)) File.Delete(target);
        if (File.Exists(target + ".tmp")) File.Delete(target + ".tmp");

        bool HasAsset(string relative)
        {
            if (!relative.StartsWith("Content/", StringComparison.Ordinal) ||
                relative.Contains("..", StringComparison.Ordinal) ||
                relative.Contains('\\') || relative.Contains(':') ||
                relative.Contains("//", StringComparison.Ordinal))
                return false;

            string path = Path.GetFullPath(Path.Combine(root,
                relative.Replace('/', Path.DirectorySeparatorChar)));
            if (!path.StartsWith(root + Path.DirectorySeparatorChar,
                StringComparison.Ordinal) || !File.Exists(path))
                return false;

            // Não usar File.Exists isoladamente: symlink em Content/Hairs
            // permite sair da pasta do mod mesmo com um caminho relativo.
            DirectoryInfo? folder = new FileInfo(path).Directory;
            while (folder != null && folder.FullName != root)
            {
                if (folder.LinkTarget is not null)
                    return false;
                folder = folder.Parent;
            }
            return new FileInfo(path).LinkTarget is null;
        }

        // Classes de uma assembly compilada, não de outras assemblies do host.
        // O Type de cada classe é atribuído pelo HairLoader a partir da reserva.
        HairManifestSession session = HairManifestSession.Prepare(
            modUid, compiledMod.GetTypes(), HasAsset, reservedFirstHairType);

        string temporary = target + ".tmp";
        try
        {
            File.WriteAllText(temporary, session.Manifest);
            File.Move(temporary, target, true);
        }
        catch
        {
            if (File.Exists(temporary)) File.Delete(temporary);
            session.Loader.Reset();
            throw;
        }
        return session;
    }
}
