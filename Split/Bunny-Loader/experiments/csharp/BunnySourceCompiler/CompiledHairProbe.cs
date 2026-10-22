using System;
using System.IO;
using System.Linq;
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
    /// <summary>
    /// Proteção na própria etapa MonoVM após compilar: o parser de fontes
    /// pode não reconhecer herança indireta ou classes criadas por geradores.
    /// Nenhum SetDefaults de outro conteúdo do mesmo mod é executado se
    /// existir cabelo C# antes de todos os hooks nativos estarem disponíveis.
    /// </summary>
    public static void RejectUninstalledHairTypes(Assembly compiledMod)
    {
        if (compiledMod is null) throw new ArgumentNullException(nameof(compiledMod));
        Type[] hairs = compiledMod.GetTypes()
            .Where(type => type.IsClass && !type.IsAbstract &&
                !type.ContainsGenericParameters && typeof(ModHair).IsAssignableFrom(type))
            .OrderBy(type => type.FullName, StringComparer.Ordinal)
            .ToArray();
        if (hairs.Length == 0) return;
        throw new InvalidOperationException(
            "[CSharp][HairLoader] NativeHairLifecycleIncomplete Classes=" +
            string.Join(",", hairs.Select(type => type.FullName)) +
            "; aguardando UI, save, rede e unload IL2CPP");
    }

    /// <summary>
    /// Ponto de entrada do MonoVM nativo após Roslyn emitir uma assembly.
    /// O UID e o primeiro ID devem vir do host, nunca de código de mod.
    /// O resultado é apenas a quantidade de descritores preparados.
    /// </summary>
    public static int ExportFromFile(
        string compiledAssemblyPath, string modUid, string modRoot,
        string privateWorkDirectory, int reservedFirstHairType)
    {
        if (string.IsNullOrWhiteSpace(compiledAssemblyPath))
            throw new ArgumentException("Assembly C# ausente.", nameof(compiledAssemblyPath));

        string path = Path.GetFullPath(compiledAssemblyPath);
        var file = new FileInfo(path);
        if (!file.Exists || file.LinkTarget is not null ||
            file.Length < 1 || file.Length > 32 * 1024 * 1024 ||
            !path.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Assembly de HairLoader inválida.");

        Assembly loaded = Assembly.Load(File.ReadAllBytes(path));
        return ExportForNative(loaded, modUid, modRoot, privateWorkDirectory,
                               reservedFirstHairType).Count;
    }

    public static HairManifestSession ExportForNative(
        Assembly compiledMod, string modUid, string modRoot,
        string privateWorkDirectory, int reservedFirstHairType)
    {
        if (string.IsNullOrEmpty(modUid) || modUid.Length > 128 ||
            modUid.Any(c => !((c >= 'a' && c <= 'z') ||
                              (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9') ||
                              c == '_' || c == '-')))
            throw new ArgumentException("UID inválido para o handoff nativo.", nameof(modUid));

        if (string.IsNullOrWhiteSpace(privateWorkDirectory))
            throw new ArgumentException("Diretório privado não informado.", nameof(privateWorkDirectory));

        string directory = Path.GetFullPath(privateWorkDirectory);
        if (!Directory.Exists(directory) ||
            new DirectoryInfo(directory).LinkTarget is not null)
            throw new InvalidOperationException("Diretório privado de manifesto indisponível.");

        string destination = Path.Combine(directory, "source-hair-" + modUid + ".manifest");
        return Export(compiledMod, modUid, modRoot, destination, reservedFirstHairType);
    }

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
        if (root.Length == 0 || !Directory.Exists(root) ||
            new DirectoryInfo(root).LinkTarget is not null)
            throw new InvalidOperationException("Diretório raiz do mod inexistente ou simbólico.");

        // SourceLoader lê os .cs de modRoot/content. As texturas do
        // ExampleModBunny estão em modRoot/content/Content/Hairs/*.png.
        // Não confundir essa raiz com a pasta privada do próprio mod.
        string contentRoot = Path.Combine(root, "content");
        if (!Directory.Exists(contentRoot) ||
            new DirectoryInfo(contentRoot).LinkTarget is not null)
            throw new InvalidOperationException(
                "A pasta content do pacote C# não existe ou é simbólica.");

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

            string path = Path.GetFullPath(Path.Combine(contentRoot,
                relative.Replace('/', Path.DirectorySeparatorChar)));
            if (!path.StartsWith(contentRoot + Path.DirectorySeparatorChar,
                StringComparison.Ordinal) || !File.Exists(path))
                return false;

            // Não usar File.Exists isoladamente: symlink em Content/Hairs
            // permite sair da pasta do mod mesmo com um caminho relativo.
            DirectoryInfo? folder = new FileInfo(path).Directory;
            while (folder != null && folder.FullName != contentRoot)
            {
                if (folder.LinkTarget is not null)
                    return false;
                folder = folder.Parent;
            }
            return folder?.FullName == contentRoot &&
                new FileInfo(path).LinkTarget is null;
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
