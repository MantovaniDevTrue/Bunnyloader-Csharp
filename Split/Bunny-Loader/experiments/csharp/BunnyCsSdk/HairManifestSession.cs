using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>
/// Resultado do reconhecimento de ModHair após a compilação Roslyn.
/// O manifesto é preparado no MonoVM; apenas o host IL2CPP poderá publicar
/// os IDs depois que texturas, UI, save e multiplayer estiverem instalados.
/// </summary>
public sealed class HairManifestSession
{
    public HairLoader Loader { get; }
    public string Manifest { get; }
    public int Count => Loader.GetDefinitions().Count;

    private HairManifestSession(HairLoader loader)
    {
        Loader = loader;
        Manifest = loader.ExportNativeManifest();
    }

    /// <summary>
    /// O SourceCompiler pode passar assembly.GetTypes() sem executar callbacks
    /// de desenho ou consulta de desbloqueio. Os tipos são ordenados pelo
    /// nome estável, independentemente da ordem de reflexão do MonoVM.
    /// O ID inicial deve ser reservado pelo host nativo ANTES da compilação.
    /// </summary>
    public static HairManifestSession Prepare(
        string modUid, IEnumerable<Type> types,
        Func<string, bool> assetExists,
        int firstModHairType = HairLoader.VanillaHairCount)
    {
        if (types is null) throw new ArgumentNullException(nameof(types));
        if (assetExists is null) throw new ArgumentNullException(nameof(assetExists));

        Type[] discovered = types
            .Where(type => type is not null &&
                type.IsClass && !type.IsAbstract &&
                typeof(ModHair).IsAssignableFrom(type))
            .OrderBy(type => type.Name, StringComparer.Ordinal)
            .ThenBy(type => type.FullName, StringComparer.Ordinal)
            .ToArray();

        if (discovered.Length == 0)
            throw new InvalidOperationException("Nenhuma classe ModHair concreta encontrada.");
        if (discovered.Length > 2048)
            throw new InvalidOperationException("Limite de classes ModHair excedido.");

        var loader = new HairLoader(firstModHairType);
        try
        {
            foreach (Type type in discovered)
            {
                if (type.ContainsGenericParameters ||
                    type.GetConstructor(Type.EmptyTypes) is null)
                    throw new InvalidOperationException(
                        type.FullName + ": ModHair requer construtor público sem parâmetros.");

                var instance = (ModHair)(Activator.CreateInstance(type)
                    ?? throw new InvalidOperationException(
                        type.FullName + ": não foi possível instanciar ModHair."));
                loader.Register(modUid, instance, assetExists);
            }
            string manifest = loader.ExportNativeManifest();
            if (manifest.Length > 512 * 1024)
                throw new InvalidOperationException("Manifesto de cabelos excede o limite nativo.");
            return new HairManifestSession(loader);
        }
        catch
        {
            // Evita deixar Type != -1 nas instâncias após falha de um lote.
            loader.Reset();
            throw;
        }
    }
}
