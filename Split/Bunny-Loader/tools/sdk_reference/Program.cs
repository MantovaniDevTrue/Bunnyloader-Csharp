using System.Reflection;
using System.Text;
using System.Text.Json;
using BunnyLoader.Source;

if (args.Length != 1 || args[0] is not ("--write" or "--check"))
    throw new ArgumentException("Run from Split/Bunny-Loader with --write or --check");

string Name(Type type)
{
    if (type.IsByRef) return "ref " + Name(type.GetElementType()!);
    if (type.IsArray) return Name(type.GetElementType()!) + "[]";
    if (type.IsGenericType)
        return type.Name.Split('`')[0] + "<" + string.Join(", ", type.GetGenericArguments().Select(Name)) + ">";
    return type == typeof(void) ? "void" : type == typeof(int) ? "int" :
        type == typeof(string) ? "string" : type == typeof(bool) ? "bool" :
        type == typeof(float) ? "float" : type.Name;
}

string Parameters(MethodBase method) => string.Join(", ", method.GetParameters()
    .Select(parameter => (parameter.IsOut ? "out " + Name(parameter.ParameterType.GetElementType()!) : Name(parameter.ParameterType)) + " " + parameter.Name));

string MethodName(MethodInfo method) => method.Name + (method.IsGenericMethodDefinition
    ? "<" + string.Join(", ", method.GetGenericArguments().Select(Name)) + ">" : "");

string Mode(Type type, MethodInfo method)
{
    if (type == typeof(BunnyRecipeBehavior) || type == typeof(BunnyRecipeRuntime) || type == typeof(BunnyRecipeLoader)) return "worker_sync_recipe_snapshot";
    if (type == typeof(BunnyShootRuntime) || type == typeof(BunnyShootSource)) return "worker_sync_shoot_snapshot";
    if (type == typeof(BunnyShootContext) || type == typeof(BunnyShot)) return "worker_shoot_command_batch";
    if (type == typeof(BunnyCraftContext)) return "worker_craft_command_queue";
    if (type == typeof(BunnyRecipeCondition)) return "managed_predicate_composition";
    if (type == typeof(BunnyNativeScope) || type == typeof(BunnyNativeType) || type == typeof(BunnyNativeMember) ||
        type == typeof(BunnyNativeBatch) || type == typeof(BunnyNativeObject)) return "experimental_game_thread_access";
    if (type == typeof(BunnyVoidHook) || type == typeof(BunnyHooks) || type == typeof(BunnyOriginalVoid) || type == typeof(BunnyMethodHook) ||
        type == typeof(BunnyHookScope) || type == typeof(BunnyHookModule) || type == typeof(BunnyUniversalHookDispatcher))
        return "experimental_worker_sync_void0";
    if (type == typeof(BunnyPlayer))
        return method.Name switch {
            "ModifyHurt" or "ModifyHitByNPC" or "ModifyHitByProjectile" or "FreeDodge" => "worker_sync_pre_hit",
            "OnHurt" or "OnHitByNPC" or "OnHitByProjectile" => "worker_async_post_hit",
            "ResetEffects" or "UpdateEquips" => "compile_policy_and_worker_hurt",
            "RegisterHooks" => "worker_registration",
            _ => "compile_policy"
        };
    if (type == typeof(BunnyEntitySourceDispatcher)) return "worker_async_entity";
    if (type == typeof(BunnyNPC) && method.Name is "OnSpawn" or "AI" or "OnHit" or "OnKill" or "OnDespawn")
        return "worker_async_entity";
    if (type == typeof(BunnyProjectile) && method.GetParameters().Any(parameter =>
        parameter.ParameterType == typeof(BunnyProjectileAiState).MakeByRefType()))
        return "worker_async_entity";
    if (type == typeof(BunnyProjectile) && method.Name != "SetDefaults") return "compile_sampled_policy";
    if (type == typeof(BunnyPrefix) &&
        method.Name is "CanRoll" or "RollChance" or "AllStatChangesHaveEffectOn" or "GetTooltipLines" ||
        type == typeof(BunnyPrefix) && method.Name == "Apply" && method.GetParameters().Length == 2 ||
        type == typeof(BunnyPrefix) && method.Name == "ApplyAccessoryEffects" && method.GetParameters().Length == 2)
        return "worker_callback_opt_in";
    if (type == typeof(BunnyPrefixRuntime) || type == typeof(BunnyItemPrefixRuntime))
        return "worker_dispatcher";
    if ((type == typeof(BunnyItem) || type == typeof(BunnyWeapon) ||
         type == typeof(BunnyAccessory) || type == typeof(BunnyBuffPotion)) &&
        method.Name == "AllowPrefix") return "worker_callback_isolated";
    if (type == typeof(BunnyPrefix) || type == typeof(BunnyPrefixRollRules) ||
        type == typeof(BunnyItemPrefixRules) ||
        type == typeof(BunnyPrefixItemConditions)) return "compile_policy";
    if ((type == typeof(BunnyItem) || type == typeof(BunnyWeapon) ||
         type == typeof(BunnyAccessory) || type == typeof(BunnyBuffPotion)) &&
        method.Name == "ApplyPrefix") return "compile_policy";
    if (method.Name is "SetDefaults" or "SetStaticDefaults" or "AddRecipes" or "NumDust") return "compile_policy";
    if (type == typeof(BunnyVisual)) return "managed_command_queue";
    return "managed_helper";
}

const BindingFlags declared = BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly;
var sdk = typeof(BunnyItem).Assembly;
var types = sdk.GetExportedTypes().OrderBy(type => type.FullName, StringComparer.Ordinal).Select(type => new {
    name = type.FullName!,
    kind = type.IsEnum ? "enum" : type.IsValueType ? "struct" : type.IsAbstract ? "abstract class" : "class",
    baseType = type.BaseType?.FullName,
    constructors = type.GetConstructors(declared).Select(method => type.Name + "(" + Parameters(method) + ")")
        .OrderBy(value => value, StringComparer.Ordinal).ToArray(),
    properties = type.GetProperties(declared).Select(property => Name(property.PropertyType) + " " + property.Name +
        " { get;" + (property.SetMethod?.IsPublic == true ? " set;" : "") + " }")
        .OrderBy(value => value, StringComparer.Ordinal).ToArray(),
    values = type.IsEnum ? Enum.GetNames(type) : Array.Empty<string>(),
    methods = type.GetMethods(declared).Where(method => !method.IsSpecialName).Select(method => new {
        signature = (method.IsStatic ? "static " : "") + Name(method.ReturnType) + " " + MethodName(method) + "(" + Parameters(method) + ")",
        execution = Mode(type, method)
    }).OrderBy(value => value.signature, StringComparer.Ordinal).ToArray()
}).ToArray();
var report = new {
    schemaVersion = 1,
    sourceProject = "experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj",
    warning = "Public SDK surface and execution contracts. Compilation is not Android validation. Managed entity AI is experimental, asynchronous, and requires native Android validation. Multiplayer is not implied.",
    types
};
var json = JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n";
var markdown = new StringBuilder("# Public C# SDK reference\n\n");
markdown.AppendLine("Generated from the compiled BunnySourceSdk. Update with `dotnet run --project tools/sdk_reference -c Release -- --write`.\n");
markdown.AppendLine("Execution terms are defined in [the SDK guide](../../../../docs/SDK-CSharp.md). Public methods do not imply Android validation, arbitrary per-frame AI, or multiplayer support. Only members declared on each type are listed; inherited members follow its base type.\n");
foreach (var type in types)
{
    markdown.AppendLine("## " + type.name + "\n");
    markdown.AppendLine(type.kind + (type.baseType == null ? "" : "; base: `" + type.baseType + "`") + ".\n");
    if (type.values.Length != 0) markdown.AppendLine("Values: " + string.Join(", ", type.values) + ".\n");
    if (type.constructors.Length + type.properties.Length != 0)
    {
        markdown.AppendLine("```csharp");
        foreach (string member in type.constructors.Concat(type.properties)) markdown.AppendLine(member);
        markdown.AppendLine("```\n");
    }
    if (type.methods.Length == 0) continue;
    markdown.AppendLine("| Method | Execution |\n| --- | --- |");
    foreach (var method in type.methods)
        markdown.AppendLine("| `" + method.signature + "` | " + method.execution + " |");
    markdown.AppendLine();
}
var outputs = new Dictionary<string, string> {
    ["docs/generated/sdk_public_reference.json"] = json,
    ["docs/generated/sdk_public_reference.md"] = markdown.ToString().Replace("\r\n", "\n")
};
foreach (var output in outputs)
{
    if (args[0] == "--check")
    {
        string actual = File.Exists(output.Key) ? File.ReadAllText(output.Key) : "";
        if (actual != output.Value)
        {
            int different = 0;
            while (different < actual.Length && different < output.Value.Length &&
                   actual[different] == output.Value[different]) different++;
            int first = Math.Max(0, different - 80);
            int actualLength = Math.Min(230, actual.Length - first);
            int expectedLength = Math.Min(230, output.Value.Length - first);
            Console.Error.WriteLine("SDK_REFERENCE_DIFFERENCE path=" + output.Key +
                " first=" + different + " actualLength=" + actual.Length +
                " expectedLength=" + output.Value.Length);
            Console.Error.WriteLine("SDK_REFERENCE_ACTUAL=" +
                JsonSerializer.Serialize(actual.Substring(first, actualLength)));
            Console.Error.WriteLine("SDK_REFERENCE_EXPECTED=" +
                JsonSerializer.Serialize(output.Value.Substring(first, expectedLength)));
            throw new InvalidOperationException("Stale public SDK reference: " + output.Key);
        }
    }
    else File.WriteAllText(output.Key, output.Value);
}
Console.WriteLine("BUNNY_PUBLIC_SDK_REFERENCE=PASS types=" + types.Length);
