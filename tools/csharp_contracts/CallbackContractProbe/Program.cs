using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using BunnyLoader.Source;

if (args.Length != 1 || !File.Exists(args[0]))
    throw new ArgumentException("Supply the CSharp managed callback manifest");

using var document = JsonDocument.Parse(File.ReadAllText(args[0]));
var root = document.RootElement;
if (root.GetProperty("schemaVersion").GetInt32() != 1)
    throw new InvalidOperationException("Unsupported callback contract schema");

var sdk = typeof(BunnyItem).Assembly;
int checkedCallbacks = 0;
foreach (var group in root.GetProperty("content").EnumerateArray())
{
    var name = group.GetProperty("type").GetString();
    var type = sdk.GetType("BunnyLoader.Source." + name, throwOnError: true);
    if (type is null || !type.IsAbstract)
        throw new InvalidOperationException("Missing abstract SDK content base: " + name);
    var keys = new HashSet<string>(StringComparer.Ordinal);
    foreach (var callback in group.GetProperty("callbacks").EnumerateArray())
    {
        var methodName = callback.GetProperty("name").GetString()!;
        var arguments = callback.GetProperty("args").EnumerateArray().Select(e => e.GetString()!).ToArray();
        var returned = callback.GetProperty("returns").GetString()!;
        var key = methodName + "(" + string.Join(",", arguments) + ")";
        if (!keys.Add(key))
            throw new InvalidOperationException("Duplicate SDK contract callback " + type.Name + "." + key);
        var methods = type.GetMethods(BindingFlags.Public | BindingFlags.Instance)
            .Where(m => m.Name == methodName
                        && m.GetParameters().Select(p => p.ParameterType.FullName).SequenceEqual(arguments))
            .ToArray();
        if (methods.Length != 1)
            throw new InvalidOperationException("SDK callback missing or ambiguous: " + type.Name + "." + key);
        var method = methods[0];
        if (!method.IsVirtual || method.ReturnType.FullName != returned)
            throw new InvalidOperationException("SDK callback virtual/return drift: " + type.Name + "." + key);
        if (callback.TryGetProperty("abstract", out var expected) && method.IsAbstract != expected.GetBoolean())
            throw new InvalidOperationException("SDK abstract callback drift: " + type.Name + "." + key);
        checkedCallbacks++;
    }
    Console.WriteLine($"CONTENT_CALLBACKS {type.Name} verified={keys.Count}");
}
Console.WriteLine($"CONTENT_CALLBACK_CONTRACT_PASS {checkedCallbacks}");
