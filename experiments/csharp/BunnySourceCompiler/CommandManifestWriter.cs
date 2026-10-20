using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

namespace BunnyLoader.SourceCompiler;

/// <summary>
/// Emite descritores sem executar Action. O parser C++ encontra classes por fonte;
/// este verificador confere as propriedades REAIS do assembly compilado.
/// A API do mod continua 100% C#, com a thread MonoVM como proprietaria.
/// </summary>
public static class CommandManifestWriter
{
    public const int MaxCommands = 128;
    public const int MaxAliases = 16;
    public const int MaxDescriptorBytes = 96 * 1024;

    public static int CountCommands(Assembly assembly) => CommandTypes(assembly).Length;

    public static string Build(Assembly assembly)
    {
        ArgumentNullException.ThrowIfNull(assembly);
        Type[] types = CommandTypes(assembly);
        if (types.Length > MaxCommands)
            throw new InvalidOperationException("BunnyCommand excede 128 classes por mod");
        var lines = new List<string> { "BUNNY_COMMANDS_V1", "CLASSES\t" + types.Length.ToString(CultureInfo.InvariantCulture) };
        var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (Type type in types)
        {
            if (type.GetConstructor(Type.EmptyTypes) == null)
                throw new InvalidOperationException(type.FullName + ": BunnyCommand precisa de construtor publico vazio");
            object instance = Activator.CreateInstance(type)
                ?? throw new InvalidOperationException(type.FullName + ": nao foi possivel instanciar BunnyCommand");
            string command = Read<string>(instance, "Command");
            int routes = Convert.ToInt32(Read<object>(instance, "Type"), CultureInfo.InvariantCulture);
            if (routes < 1 || (routes & ~7) != 0)
                throw new InvalidOperationException(type.FullName + ": tipo de comando invalido");
            string name = Normalize(command);
            if (!used.Add(name))
                throw new InvalidOperationException(type.FullName + ": comando duplicado: " + name);
            lines.Add("CMD\t" + type.FullName + "\t" + name + "\t" + routes.ToString(CultureInfo.InvariantCulture));
            var aliases = Read<object>(instance, "Aliases") as IEnumerable
                ?? throw new InvalidOperationException(type.FullName + ": aliases devem ser enumeraveis");
            var aliasNames = new List<string>();
            foreach (object? value in aliases)
            {
                if (value is not string alias)
                    throw new InvalidOperationException(type.FullName + ": alias precisa ser string");
                if (aliasNames.Count >= MaxAliases)
                    throw new InvalidOperationException(type.FullName + ": aliases excedem 16");
                string normalized = Normalize(alias);
                if (!used.Add(normalized))
                    throw new InvalidOperationException(type.FullName + ": alias duplicado: " + normalized);
                aliasNames.Add(normalized);
            }
            foreach (string alias in aliasNames)
                lines.Add("ALIAS\t" + type.FullName + "\t" + alias + "\t" + routes.ToString(CultureInfo.InvariantCulture));
        }
        string wire = string.Join("\n", lines) + "\n";
        if (Encoding.UTF8.GetByteCount(wire) > MaxDescriptorBytes)
            throw new InvalidOperationException("BunnyCommand: manifesto maior que 96 KiB");
        return wire;
    }

    public static int Write(Assembly assembly, string diagnosticsPath)
    {
        string manifest = Build(assembly);
        string dir = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Diretorio de diagnosticos C# ausente");
        string target = Path.Combine(dir, "source-commands.txt");
        string temporary = target + ".tmp";
        File.WriteAllText(temporary, manifest, new UTF8Encoding(false));
        File.Move(temporary, target, true);
        return CountCommands(assembly);
    }

    private static Type[] CommandTypes(Assembly assembly)
    {
        ArgumentNullException.ThrowIfNull(assembly);
        return assembly.GetTypes()
            .Where(type => type.IsClass && !type.IsAbstract && !type.ContainsGenericParameters &&
                           (type.IsPublic || type.IsNestedPublic) &&
                           Ancestors(type).Any(a => a.FullName == "BunnyLoader.Source.BunnyCommand"))
            .OrderBy(type => type.FullName, StringComparer.Ordinal).ToArray();
    }

    private static IEnumerable<Type> Ancestors(Type type)
    {
        for (Type? ancestor = type.BaseType; ancestor != null; ancestor = ancestor.BaseType)
            yield return ancestor;
    }

    private static T Read<T>(object instance, string property)
    {
        PropertyInfo? info = instance.GetType().GetProperty(property,
            BindingFlags.Public | BindingFlags.Instance);
        object? value = info?.GetValue(instance);
        if (value is not T result)
            throw new InvalidOperationException(instance.GetType().FullName + ": propriedade " + property + " invalida");
        return result;
    }

    private static string Normalize(string raw)
    {
        if (raw == null) throw new InvalidOperationException("BunnyCommand: nome nulo");
        string result = raw.Trim().TrimStart('/').ToLowerInvariant();
        if (result.Length == 0 || result.Length > 64 ||
            result.Any(ch => !((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                               ch == '_' || ch == '-' || ch == '.')))
            throw new InvalidOperationException("BunnyCommand: nome ou alias invalido: " + raw);
        return result;
    }
}
