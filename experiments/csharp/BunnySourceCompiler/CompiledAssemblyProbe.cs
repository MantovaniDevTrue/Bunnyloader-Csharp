using System;
using System.IO;
using System.Globalization;
using System.Reflection;
using System.Reflection.Emit;
using System.Linq;
using System.Collections.Generic;

namespace BunnyLoader.SourceCompiler;

public static class CompiledAssemblyProbe
{
    private static bool IsContentType(Type type, string baseName)
    {
        if (!type.IsClass || type.IsAbstract || type.ContainsGenericParameters ||
            !Ancestors(type).Any(ancestor => ancestor.FullName == "BunnyLoader.Source." + baseName))
            return false;
        var declaration = Declaration(type);
        if (declaration != null)
        {
            string kind = declaration.GetType().GetProperty("Kind")!.GetValue(declaration)!.ToString()!;
            if ("Bunny" + kind != baseName)
                throw new InvalidOperationException(type.FullName + ": BunnyContent kind does not match " + baseName);
        }
        else if (type.BaseType?.FullName != "BunnyLoader.Source." + baseName)
            throw new InvalidOperationException(type.FullName +
                ": inherited content requires [BunnyContent(BunnyContentKind." +
                baseName.Substring(5) + ", displayName, texture)] for native registration");
        return true;
    }

    private static object? Declaration(Type type) => type.GetCustomAttributes(false)
        .SingleOrDefault(attribute => attribute.GetType().FullName ==
            "BunnyLoader.Source.BunnyContentAttribute");

    private static void ApplyDeclaration(object instance)
    {
        Type type = instance.GetType();
        object? declaration = Declaration(type);
        if (declaration == null) return;
        foreach (string property in new[] { "DisplayName", "Texture" })
        {
            PropertyInfo? target = type.GetProperty(property);
            if (target == null) continue;
            target.SetValue(instance, declaration.GetType().GetProperty(property)!.GetValue(declaration));
        }
    }

    private static void ValidateDeclaration(object instance)
    {
        Type type = instance.GetType();
        object? declaration = Declaration(type);
        if (declaration == null) return;
        foreach (string property in new[] { "DisplayName", "Texture" })
        {
            PropertyInfo? target = type.GetProperty(property);
            if (target != null && !Equals(target.GetValue(instance),
                declaration.GetType().GetProperty(property)!.GetValue(declaration)))
                throw new InvalidOperationException(type.FullName + ": SetDefaults changed declared " + property);
        }
    }

    private static IEnumerable<Type> Ancestors(Type type)
    {
        for (Type? ancestor = type.BaseType; ancestor != null; ancestor = ancestor.BaseType)
            yield return ancestor;
    }

    private static bool UsesLiveLocalizationRegister(Type[] types)
    {
        var opcodes = new Dictionary<ushort, OpCode>();
        foreach (FieldInfo field in typeof(OpCodes).GetFields(BindingFlags.Public | BindingFlags.Static))
            if (field.GetValue(null) is OpCode code)
                opcodes[unchecked((ushort)code.Value)] = code;
        const BindingFlags flags = BindingFlags.Public | BindingFlags.NonPublic |
            BindingFlags.Static | BindingFlags.Instance | BindingFlags.DeclaredOnly;
        foreach (Type type in types)
        {
            foreach (MethodBase method in type.GetMethods(flags).Cast<MethodBase>()
                .Concat(type.GetConstructors(flags)))
            {
                byte[]? il = method.GetMethodBody()?.GetILAsByteArray();
                if (il == null) continue;
                int position = 0;
                while (position < il.Length)
                {
                    ushort raw = il[position++];
                    if (raw == 0xFE)
                    {
                        if (position >= il.Length) break;
                        raw = (ushort)(0xFE00 | il[position++]);
                    }
                    if (!opcodes.TryGetValue(raw, out OpCode code)) break;
                    int operand = code.OperandType switch
                    {
                        OperandType.InlineNone => 0,
                        OperandType.ShortInlineBrTarget or OperandType.ShortInlineI or
                            OperandType.ShortInlineVar => 1,
                        OperandType.InlineVar => 2,
                        OperandType.InlineI or OperandType.InlineBrTarget or
                            OperandType.InlineField or OperandType.InlineMethod or
                            OperandType.InlineSig or OperandType.InlineString or
                            OperandType.InlineTok or OperandType.InlineType or
                            OperandType.ShortInlineR => 4,
                        OperandType.InlineI8 or OperandType.InlineR => 8,
                        OperandType.InlineSwitch when position + 4 <= il.Length =>
                            4 + 4 * Math.Clamp(BitConverter.ToInt32(il, position), 0,
                                (il.Length - position - 4) / 4),
                        _ => -1
                    };
                    if (operand < 0 || position + operand > il.Length) break;
                    if (code.OperandType == OperandType.InlineMethod &&
                        (raw == unchecked((ushort)OpCodes.Call.Value) ||
                         raw == unchecked((ushort)OpCodes.Callvirt.Value)))
                    {
                        int token = BitConverter.ToInt32(il, position);
                        try
                        {
                            MethodBase? target = method.Module.ResolveMethod(token,
                                type.IsGenericType ? type.GetGenericArguments() : null,
                                method.IsGenericMethod ? method.GetGenericArguments() : null);
                            if (target?.DeclaringType?.FullName == "BunnyLoader.Source.BunnyLocalizationRuntime" &&
                                target.Name == "Register") return true;
                        }
                        catch (ArgumentException) { }
                        catch (BadImageFormatException) { }
                        catch (NotSupportedException) { }
                        catch (TypeLoadException) { }
                        catch (FileNotFoundException) { }
                    }
                    position += operand;
                }
            }
        }
        return false;
    }

    public static int Verify(string assemblyPath, string diagnosticsPath)
        => Verify(assemblyPath, diagnosticsPath, "");

    public static int Verify(string assemblyPath, string diagnosticsPath, string modRoot)
    {
        try
        {
            if (!File.Exists(assemblyPath) || new FileInfo(assemblyPath).Length > 32 * 1024 * 1024)
                throw new InvalidOperationException("Compiled assembly missing or too large");

            Assembly assembly = Assembly.Load(File.ReadAllBytes(assemblyPath));
            Type[] contentTypes = assembly.GetTypes();
            Type? console = assembly.GetType("BunnyLoader.Interactive.ConsoleEntry", false);
            if (console != null)
            {
                MethodInfo? run = console.GetMethod("Run", BindingFlags.Public | BindingFlags.Static,
                    null, Type.EmptyTypes, null);
                if (contentTypes.Length != 1 || !console.IsClass ||
                    !console.IsAbstract || !console.IsSealed ||
                    run == null || run.ReturnType != typeof(string))
                    throw new InvalidOperationException("CSharp console assembly contract invalid");
                File.AppendAllText(diagnosticsPath, "\nINTERACTIVE_CONSOLE_VERIFIED classes=1");
                return 1;
            }
            var projectileTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyProjectile"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var wallTypes = contentTypes.Where(type => IsContentType(type, "BunnyWall")).OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var tileTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyTile"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var dustTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyDust"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var basicItemTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyItem"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var weaponTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyWeapon"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var buffTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyBuff"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var potionTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyBuffPotion"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var accessoryTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyAccessory"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var playerTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyPlayer"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var npcTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyNPC"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var prefixTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyPrefix"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var localizationTypes = contentTypes
                .Where(type => type.IsClass && !type.IsAbstract && !type.ContainsGenericParameters &&
                    type.BaseType?.FullName == "BunnyLoader.Source.BunnyLocalizationModule")
                .OrderBy(type => type.FullName, StringComparer.Ordinal).ToArray();
            var rarityTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyRarity"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var screenEffectTypes = contentTypes
                .Where(type => IsContentType(type, "BunnyScreenEffect"))
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            int commandCount = CommandManifestWriter.CountCommands(assembly);
            if (dustTypes.Length > 4096 || wallTypes.Length > 4096 || tileTypes.Length > 4096 || rarityTypes.Length > 4096 || localizationTypes.Length > 32 || prefixTypes.Length > 4096 || npcTypes.Length > 4096 || accessoryTypes.Length > 4096 || playerTypes.Length > 4096 ||
                projectileTypes.Length > 4096 ||
                weaponTypes.Length > 4096 || basicItemTypes.Length > 4096 ||
                buffTypes.Length > 4096 || potionTypes.Length > 4096 || screenEffectTypes.Length > 4096)
                throw new InvalidOperationException("C# content class count out of bounds");
            if (dustTypes.Length + wallTypes.Length + tileTypes.Length + basicItemTypes.Length + projectileTypes.Length + weaponTypes.Length + buffTypes.Length + potionTypes.Length + accessoryTypes.Length + playerTypes.Length + npcTypes.Length + prefixTypes.Length + screenEffectTypes.Length + rarityTypes.Length + localizationTypes.Length + commandCount == 0)
                throw new InvalidOperationException("C# assembly has no supported content types");
            bool fixture = projectileTypes.Any(t => t.FullName == "SourceCSharpMod.SourceBolt") &&
                weaponTypes.Any(t => t.FullName == "SourceCSharpMod.SourceWand");
            object? primaryProjectile = fixture ? VerifyProjectile(assembly) : null;
            object? primaryWeapon = fixture ? VerifyWeapon(assembly) : null;
            object[] projectiles = projectileTypes.Select(type =>
                fixture && type.FullName == "SourceCSharpMod.SourceBolt"
                    ? primaryProjectile! : CreateContent(type)).ToArray();
            object[] weapons = weaponTypes.Select(type =>
                fixture && type.FullName == "SourceCSharpMod.SourceWand"
                    ? primaryWeapon! : CreateContent(type)).ToArray();
            object[] basicItems = basicItemTypes.Select(CreateContent).ToArray();
            object[] tiles = tileTypes.Select(CreateTileContent).ToArray();
            object[] walls = wallTypes.Select(CreateTileContent).ToArray();
            object[] dusts = dustTypes.Select(CreateTileContent).ToArray();
            object[] buffs = buffTypes.Select(CreateContent).ToArray();
            object[] potions = potionTypes.Select(CreateContent).ToArray();
            object[] accessories = accessoryTypes.Select(CreateContent).ToArray();
            object[] players = playerTypes.Select(CreateContent).ToArray();
            object[] npcs = npcTypes.Select(CreateContent).ToArray();
            object[] prefixes = prefixTypes.Select(CreateContent).ToArray();
            object[] screenEffects = screenEffectTypes.Select(CreateContent).ToArray();
            foreach (object effect in screenEffects)
            {
                string shader = (string)effect.GetType().GetProperty("ShaderFile")!.GetValue(effect)!;
                string pipeline = (string)effect.GetType().GetProperty("PipelineFile")!.GetValue(effect)!;
                if (string.IsNullOrWhiteSpace(shader) == string.IsNullOrWhiteSpace(pipeline))
                    throw new InvalidOperationException(effect.GetType().FullName + ": exactly one ShaderFile or PipelineFile required");
            }
            WriteRuntimeDefaults(projectiles, weapons, buffs, potions,
                accessories, players, npcs, prefixes, basicItems, tiles, walls, dusts, rarityTypes, diagnosticsPath);
            commandCount = CommandManifestWriter.Write(assembly, diagnosticsPath, assemblyPath);
            if (modRoot.Length != 0)
                AppendManagedLocalization(modRoot, diagnosticsPath, localizationTypes,
                    UsesLiveLocalizationRegister(contentTypes));

            WriteProjectileMotionPolicies(assembly, projectileTypes, diagnosticsPath);
            WriteAutomaticCombatPolicies(projectiles, diagnosticsPath);
            if (fixture)
            {
                WriteProjectileMotion(assembly, diagnosticsPath);
                WriteSparkMotion(assembly, diagnosticsPath);
                WriteHomingPolicy(assembly, diagnosticsPath);
                WriteHitNPCPolicy(primaryProjectile!, diagnosticsPath);
                WriteKillPolicy(primaryProjectile!, diagnosticsPath);
            }
            File.AppendAllText(diagnosticsPath,
                "\nASSEMBLY_EXEC_OK classes=" + (dusts.Length + walls.Length + tiles.Length + projectiles.Length + weapons.Length + buffs.Length + potions.Length + accessories.Length + players.Length + npcs.Length + prefixes.Length + rarityTypes.Length + basicItems.Length + screenEffects.Length) +
                " projectiles=" + projectiles.Length + " weapons=" + weapons.Length +
                " buffs=" + buffs.Length + " potions=" + potions.Length +
                " accessories=" + accessories.Length + " players=" + players.Length +
                " dusts=" + dusts.Length + " npcs=" + npcs.Length + " tiles=" + tiles.Length + " prefixes=" + prefixes.Length +
                " rarities=" + rarityTypes.Length +
                " screenEffects=" + screenEffects.Length + " localizationModules=" + localizationTypes.Length + " commands=" + commandCount +
                (fixture
                    ? " projectile=SourceBolt width=10 weapon=SourceWand damage=22 shoot=SourceBolt"
                    : " policy=generic-managed-defaults"));
            return projectiles.Length + weapons.Length + buffs.Length + potions.Length +
                accessories.Length + players.Length + npcs.Length + prefixes.Length + basicItems.Length + tiles.Length + dusts.Length + walls.Length + screenEffects.Length + rarityTypes.Length + localizationTypes.Length + commandCount;
        }
        catch (Exception exception)
        {
            File.AppendAllText(diagnosticsPath,
                "\nASSEMBLY_EXEC_FAILED " + exception.ToString()[..Math.Min(3000, exception.ToString().Length)]);
            return -1;
        }
    }


    private static void AppendManagedLocalization(string modRoot, string diagnosticsPath,
        Type[] modules, bool liveRegister)
    {
        string source = Path.Combine(modRoot, "content");
        var catalog = new BunnyLoader.Source.BunnyLocalizationCatalog();
        int cultures = catalog.LoadFromContent("source", source);
        foreach (Type type in modules)
        {
            if (type.GetConstructor(Type.EmptyTypes) == null)
                throw new InvalidOperationException("BunnyLocalizationModule requires a public parameterless constructor: " + type.FullName);
            var module = Activator.CreateInstance(type) as BunnyLoader.Source.BunnyLocalizationModule
                ?? throw new InvalidOperationException("Failed to instantiate localization module: " + type.FullName);
            module.RegisterTranslations(catalog);
        }

        if (cultures == 0 && modules.Length == 0 && !liveRegister) return;
        foreach (string key in catalog.SnapshotForGame().Keys)
            if (key.StartsWith("Mods.", StringComparison.Ordinal) &&
                !key.StartsWith("Mods.source.", StringComparison.Ordinal))
                throw new InvalidOperationException(
                    "BunnyLocalizationModule cannot register keys owned by other mods: " + key);

        IReadOnlyList<string> rows = catalog.ExportWireRows();
        if (rows.Count > 16384)
            throw new InvalidDataException("Localization wire exceeds row limit");
        string output = Path.Combine(Path.GetDirectoryName(diagnosticsPath) ??
            throw new InvalidOperationException("Missing diagnostics directory"), "source-runtime-values.txt");
        using (var stream = new FileStream(output, FileMode.Append, FileAccess.Write, FileShare.None))
        using (var writer = new StreamWriter(stream, new System.Text.UTF8Encoding(false)))
        {
            if (modules.Length != 0)
                writer.WriteLine("LOCALE_MODULES\t" + modules.Length);
            if (liveRegister)
                writer.WriteLine("LOCALE_RUNTIME\t1");
            foreach (string row in rows)
                writer.WriteLine(row);
        }
        File.AppendAllText(diagnosticsPath,
            "\nMANAGED_LOCALIZATION_WIRE cultures=" + cultures +
            " modules=" + modules.Length + " rows=" + rows.Count);
    }


    private static object CreateContent(Type type)
    {
        object instance = Activator.CreateInstance(type)
            ?? throw new InvalidOperationException("Failed to instantiate " + type.Name);
        ApplyDeclaration(instance);
        MethodInfo method = type.GetMethod("SetDefaults", BindingFlags.Public | BindingFlags.Instance)
            ?? throw new InvalidOperationException("Missing SetDefaults in " + type.Name);
        method.Invoke(instance, null);
        ValidateDeclaration(instance);
        return instance;
    }

    private static int DustHookMask(Type type)
    {
        Type dust = Ancestors(type).First(parent => parent.FullName == "BunnyLoader.Source.BunnyDust");
        Type state = dust.Assembly.GetType("BunnyLoader.Source.BunnyDustState", throwOnError: true)!;
        Type stateRef = state.MakeByRefType();
        Type colorRef = typeof(uint).MakeByRefType();
        bool Overrides(string name, params Type[] args)
        {
            MethodInfo? method = type.GetMethod(name, BindingFlags.Public | BindingFlags.Instance,
                binder: null, types: args, modifiers: null);
            return method != null && method.DeclaringType != dust;
        }
        return (Overrides("OnSpawn", stateRef) ? 1 : 0) |
               (Overrides("Update", stateRef) ? 2 : 0) |
               (Overrides("MidUpdate", stateRef) ? 4 : 0) |
               (Overrides("GetAlpha", stateRef, colorRef) ? 8 : 0) |
               (Overrides("PreDraw", stateRef) ? 16 : 0);
    }

    private static object CreateTileContent(Type type)
    {
        object instance = Activator.CreateInstance(type)
            ?? throw new InvalidOperationException("Failed to instantiate tile " + type.FullName);
        ApplyDeclaration(instance);
        MethodInfo method = type.GetMethod("SetStaticDefaults", BindingFlags.Public | BindingFlags.Instance)
            ?? throw new InvalidOperationException("Missing tile SetStaticDefaults in " + type.FullName);
        method.Invoke(instance, null);
        ValidateDeclaration(instance);
        return instance;
    }

    private static object VerifyProjectile(Assembly assembly)
    {
        Type type = assembly.GetType("SourceCSharpMod.SourceBolt", throwOnError: true)!;
        if (type.BaseType?.FullName != "BunnyLoader.Source.BunnyProjectile")
            throw new InvalidOperationException("SourceBolt base type invalid");
        object instance = Activator.CreateInstance(type)!;
        type.GetMethod("SetDefaults", BindingFlags.Public | BindingFlags.Instance)!.Invoke(instance, null);
        if ((int)type.GetProperty("Width")!.GetValue(instance)! != 10 ||
            (int)type.GetProperty("Height")!.GetValue(instance)! != 10 ||
            (int)type.GetProperty("AiStyle")!.GetValue(instance)! != 1 ||
            !(bool)type.GetProperty("Friendly")!.GetValue(instance)!)
            throw new InvalidOperationException("SourceBolt SetDefaults did not execute correctly");
        return instance;
    }

    private static object VerifyWeapon(Assembly assembly)
    {
        Type type = assembly.GetType("SourceCSharpMod.SourceWand", throwOnError: true)!;
        if (type.BaseType?.FullName != "BunnyLoader.Source.BunnyWeapon")
            throw new InvalidOperationException("SourceWand base type invalid");
        object instance = Activator.CreateInstance(type)!;
        type.GetMethod("SetDefaults", BindingFlags.Public | BindingFlags.Instance)!.Invoke(instance, null);
        if ((int)type.GetProperty("Damage")!.GetValue(instance)! != 22 ||
            (int)type.GetProperty("UseTime")!.GetValue(instance)! != 18 ||
            (float)type.GetProperty("ShootSpeed")!.GetValue(instance)! != 13f ||
            (string)type.GetProperty("Shoot")!.GetValue(instance)! != "SourceBolt")
            throw new InvalidOperationException("SourceWand SetDefaults did not execute correctly");
        return instance;
    }
    private static void WriteHitNPCPolicy(object projectile, string diagnosticsPath)
    {
        Type type = projectile.GetType();
        MethodInfo hit = type.GetMethod("OnHitNPC", BindingFlags.Public | BindingFlags.Instance,
            null, new[] { typeof(int) }, null)
            ?? throw new InvalidOperationException("SourceBolt.OnHitNPC(int) missing");
        MethodInfo buff = type.GetMethod("OnHitBuffId", BindingFlags.Public | BindingFlags.Instance)
            ?? throw new InvalidOperationException("SourceBolt.OnHitBuffId missing");
        if (hit.ReturnType != typeof(int) || buff.ReturnType != typeof(int))
            throw new InvalidOperationException("C# OnHitNPC signature invalid");
        if (buff.Invoke(projectile, null) is not int buffId || buffId != 24)
            throw new InvalidOperationException("Only vanilla OnFire buff id 24 is allowed for this test");
        string[] turns = new string[256];
        for (int i = 0; i < turns.Length; ++i)
        {
            if (hit.Invoke(projectile, new object[] { i }) is not int duration ||
                duration < 30 || duration > 180)
                throw new InvalidOperationException("Invalid OnHitNPC effect duration");
            turns[i] = duration.ToString(CultureInfo.InvariantCulture);
        }
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing managed diagnostics directory");
        string path = Path.Combine(folder, "source-onhit-policy.txt");
        string temp = path + ".tmp";
        File.WriteAllText(temp, "BUNNY_SOURCE_ONHIT_V2\nSourceBolt\n24\n" +
            string.Join("\t", turns) + "\n");
        File.Move(temp, path, true);
        File.AppendAllText(diagnosticsPath, "\nONHIT_POLICY_OK buff=24 hits=256");
    }

    private static void WriteKillPolicy(object projectile, string diagnosticsPath)
    {
        Type type = projectile.GetType();
        MethodInfo radius = type.GetMethod("OnKill", BindingFlags.Public | BindingFlags.Instance,
            null, new[] { typeof(int) }, null)
            ?? throw new InvalidOperationException("SourceBolt.OnKill(int) missing");
        MethodInfo duration = type.GetMethod("OnKillDuration", BindingFlags.Public | BindingFlags.Instance,
            null, new[] { typeof(int) }, null)
            ?? throw new InvalidOperationException("SourceBolt.OnKillDuration(int) missing");
        MethodInfo buff = type.GetMethod("OnKillBuffId", BindingFlags.Public | BindingFlags.Instance)
            ?? throw new InvalidOperationException("SourceBolt.OnKillBuffId missing");
        if (radius.ReturnType != typeof(int) || duration.ReturnType != typeof(int) ||
            buff.ReturnType != typeof(int) || buff.Invoke(projectile, null) is not int buffId ||
            buffId != 24)
            throw new InvalidOperationException("C# OnKill signature or buff invalid");
        string[] radii = new string[256];
        string[] durations = new string[256];
        for (int i = 0; i < 256; ++i)
        {
            if (radius.Invoke(projectile, new object[] { i }) is not int r ||
                r < 24 || r > 120 ||
                duration.Invoke(projectile, new object[] { i }) is not int ticks ||
                ticks < 30 || ticks > 180)
                throw new InvalidOperationException("C# OnKill radius or duration invalid");
            radii[i] = r.ToString(CultureInfo.InvariantCulture);
            durations[i] = ticks.ToString(CultureInfo.InvariantCulture);
        }
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing managed diagnostics directory");
        string path = Path.Combine(folder, "source-onkill-policy.txt");
        string temp = path + ".tmp";
        File.WriteAllText(temp, "BUNNY_SOURCE_ONKILL_V1\nSourceBolt\n24\n" +
            string.Join("\t", radii) + "\n" + string.Join("\t", durations) + "\n");
        File.Move(temp, path, true);
        File.AppendAllText(diagnosticsPath, "\nONKILL_POLICY_OK buff=24 radius=256 duration=256");
    }


    private static void WriteAutomaticCombatPolicies(object[] projectiles, string diagnosticsPath)
    {
        var policies = new List<string> { "BUNNY_COMBAT_POLICIES_V1" };
        foreach (object projectile in projectiles)
        {
            Type type = projectile.GetType();
            const BindingFlags flags = BindingFlags.Public | BindingFlags.Instance;
            MethodInfo? hit = type.GetMethod("OnHitNPC", flags, null,
                new[] { typeof(int) }, null);
            if (hit?.DeclaringType == type)
            {
                MethodInfo buff = type.GetMethod("OnHitBuffId", flags, null,
                    Type.EmptyTypes, null)
                    ?? throw new InvalidOperationException(type.Name + ": OnHitBuffId missing");
                if (hit.ReturnType != typeof(int) || buff.ReturnType != typeof(int) ||
                    buff.Invoke(projectile, null) is not int buffId || buffId != 24)
                    throw new InvalidOperationException(type.Name + ": unsupported OnHitNPC buff");
                string[] durations = new string[256];
                for (int damage = 0; damage < 256; ++damage)
                {
                    if (hit.Invoke(projectile, new object[] { damage }) is not int ticks ||
                        ticks < 30 || ticks > 180)
                        throw new InvalidOperationException(type.Name + ": invalid OnHitNPC duration");
                    durations[damage] = ticks.ToString(CultureInfo.InvariantCulture);
                }
                policies.Add("HIT\t" + type.Name + "\t24\t" + string.Join("\t", durations));
            }
            MethodInfo? kill = type.GetMethod("OnKill", flags, null,
                new[] { typeof(int) }, null);
            if (kill?.DeclaringType == type)
            {
                MethodInfo duration = type.GetMethod("OnKillDuration", flags, null,
                    new[] { typeof(int) }, null)
                    ?? throw new InvalidOperationException(type.Name + ": OnKillDuration missing");
                MethodInfo buff = type.GetMethod("OnKillBuffId", flags, null,
                    Type.EmptyTypes, null)
                    ?? throw new InvalidOperationException(type.Name + ": OnKillBuffId missing");
                if (kill.ReturnType != typeof(int) || duration.ReturnType != typeof(int) ||
                    buff.ReturnType != typeof(int) ||
                    buff.Invoke(projectile, null) is not int buffId || buffId != 24)
                    throw new InvalidOperationException(type.Name + ": unsupported OnKill buff");
                string[] radii = new string[256];
                string[] durations = new string[256];
                for (int damage = 0; damage < 256; ++damage)
                {
                    if (kill.Invoke(projectile, new object[] { damage }) is not int radius ||
                        radius < 24 || radius > 120 ||
                        duration.Invoke(projectile, new object[] { damage }) is not int ticks ||
                        ticks < 30 || ticks > 180)
                        throw new InvalidOperationException(type.Name + ": invalid OnKill effect");
                    radii[damage] = radius.ToString(CultureInfo.InvariantCulture);
                    durations[damage] = ticks.ToString(CultureInfo.InvariantCulture);
                }
                policies.Add("KILL\t" + type.Name + "\t24\t" +
                    string.Join("\t", radii) + "\t" + string.Join("\t", durations));
            }
        }
        if (policies.Count > 17)
            throw new InvalidOperationException("Too many C# automatic combat effects");
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing C# diagnostics directory");
        string path = Path.Combine(folder, "source-combat-policies.txt");
        File.WriteAllText(path + ".tmp", string.Join("\n", policies) + "\n");
        File.Move(path + ".tmp", path, true);
        File.AppendAllText(diagnosticsPath,
            "\nBUNNY_AUTO_COMBAT_POLICIES_V1_OK count=" + (policies.Count - 1));
    }

    private static void WriteProjectileMotionPolicies(
        Assembly assembly, Type[] projectileTypes, string diagnosticsPath)
    {
        var policies = new List<string>();
        foreach (Type projectile in projectileTypes)
        {
            string fullname = projectile.Namespace + "." + projectile.Name + "Motion";
            Type? motionType = assembly.GetType(fullname, throwOnError: false);
            if (motionType == null) continue;
            MethodInfo? turnMethod = motionType.GetMethod("TurnAtTick",
                BindingFlags.Public | BindingFlags.Static, null, new[] { typeof(int) }, null);
            if (turnMethod == null || turnMethod.ReturnType != typeof(float))
                throw new InvalidOperationException("Invalid TurnAtTick(int) on " + fullname);
            string[] samples = new string[120];
            for (int tick = 0; tick < samples.Length; ++tick)
            {
                if (turnMethod.Invoke(null, new object[] { tick }) is not float angle ||
                    !float.IsFinite(angle) || MathF.Abs(angle) > 0.08f)
                    throw new InvalidOperationException("Invalid projectile motion from " + fullname);
                samples[tick] = ((int)MathF.Round(angle * 1000f)).ToString(CultureInfo.InvariantCulture);
            }
            policies.Add(projectile.Name + "\t" + string.Join("\t", samples));
        }
        if (policies.Count > 4096)
            throw new InvalidOperationException("More than 4096 independent projectile motion curves");
        string dir = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing diagnostics directory");
        string destination = Path.Combine(dir, "source-projectile-turns.txt");
        string temporary = destination + ".tmp";
        File.WriteAllText(temporary, "BUNNY_PROJECTILE_TURNS_V1\n" +
            string.Join("\n", policies) + "\n");
        File.Move(temporary, destination, true);
        File.AppendAllText(diagnosticsPath, "\nPROJECTILE_TURNS_V1_OK curves=" + policies.Count);
    }

    private static void WriteSparkMotion(Assembly assembly, string diagnosticsPath)
    {
        Type type = assembly.GetType("SourceCSharpMod.SourceSparkMotion", throwOnError: true)!;
        MethodInfo motion = type.GetMethod("TurnAtTick", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("SourceSparkMotion.TurnAtTick missing");
        if (motion.ReturnType != typeof(float))
            throw new InvalidOperationException("SourceSparkMotion.TurnAtTick must return float");
        string[] samples = new string[120];
        int nonzero = 0;
        for (int tick = 0; tick < samples.Length; ++tick)
        {
            if (motion.Invoke(null, new object[] { tick }) is not float radians ||
                !float.IsFinite(radians) || MathF.Abs(radians) > 0.06f)
                throw new InvalidOperationException("Invalid SourceSpark C# AI at tick " + tick);
            int milliRadians = (int)MathF.Round(radians * 1000f);
            if (milliRadians != 0) ++nonzero;
            samples[tick] = milliRadians.ToString(CultureInfo.InvariantCulture);
        }
        if (nonzero < 80)
            throw new InvalidOperationException("SourceSpark AI curve has insufficient motion");
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing diagnostics directory");
        string path = Path.Combine(folder, "source-spark-turns.txt");
        string temp = path + ".tmp";
        File.WriteAllText(temp, "BUNNY_SOURCE_SPARK_TURNS_V1\nSourceSpark\n" +
            string.Join("\t", samples) + "\n");
        File.Move(temp, path, true);
        File.AppendAllText(diagnosticsPath, "\nSPARK_AI_BUFFER_OK ticks=120 source=SourceSparkMotion");
    }

    private static void WriteProjectileMotion(Assembly assembly, string diagnosticsPath)
    {
        Type type = assembly.GetType("SourceCSharpMod.SourceBoltMotion", throwOnError: true)!;
        MethodInfo motion = type.GetMethod("TurnAtTick", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("SourceBoltMotion.TurnAtTick missing");
        if (motion.ReturnType != typeof(float))
            throw new InvalidOperationException("SourceBoltMotion.TurnAtTick must return float");
        string[] samples = new string[120];
        for (int i = 0; i < samples.Length; ++i)
        {
            object? result = motion.Invoke(null, new object[] { i });
            if (result is not float angle || !float.IsFinite(angle) || MathF.Abs(angle) > 0.08f)
                throw new InvalidOperationException("Invalid bounded C# AI angle at tick " + i);
            int milliRadians = (int)MathF.Round(angle * 1000f);
            samples[i] = milliRadians.ToString(CultureInfo.InvariantCulture);
        }
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing managed diagnostics directory");
        string destination = Path.Combine(folder, "source-ai-turns.txt");
        string temp = destination + ".tmp";
        File.WriteAllText(temp, "BUNNY_SOURCE_AI_TURNS_V1\nSourceBolt\n" +
            string.Join("\t", samples) + "\n");
        File.Move(temp, destination, true);
        File.AppendAllText(diagnosticsPath, "\nAI_BUFFER_OK ticks=120 function=SourceBoltMotion.TurnAtTick");
    }

    private static void WriteHomingPolicy(Assembly assembly, string diagnosticsPath)
    {
        Type type = assembly.GetType("SourceCSharpMod.SourceBoltMotion", throwOnError: true)!;
        MethodInfo turn = type.GetMethod("HomingTurn", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("SourceBoltMotion.HomingTurn missing");
        MethodInfo range = type.GetMethod("HomingRange", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("SourceBoltMotion.HomingRange missing");
        MethodInfo interval = type.GetMethod("RetargetInterval", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("SourceBoltMotion.RetargetInterval missing");
        int radius = (int)(range.Invoke(null, null)
            ?? throw new InvalidOperationException("HomingRange returned null"));
        int retarget = (int)(interval.Invoke(null, null)
            ?? throw new InvalidOperationException("RetargetInterval returned null"));
        if (radius < 64 || radius > 720 || retarget < 4 || retarget > 24)
            throw new InvalidOperationException("Invalid C# homing radius or retarget interval");

        string[] lookup = new string[129];
        for (int i = 0; i < lookup.Length; ++i)
        {
            float error = (i - 64) * MathF.PI / 64f;
            object? response = turn.Invoke(null, new object[] { error });
            if (response is not float angle || !float.IsFinite(angle) || MathF.Abs(angle) > 0.08f)
                throw new InvalidOperationException("Invalid C# homing angle index " + i);
            lookup[i] = ((int)MathF.Round(angle * 1000f)).ToString(CultureInfo.InvariantCulture);
        }
        if (lookup[64] != "0" ||
            !int.TryParse(lookup[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out int left) ||
            !int.TryParse(lookup[128], NumberStyles.Integer, CultureInfo.InvariantCulture, out int right) ||
            left >= 0 || right <= 0)
            throw new InvalidOperationException("Homing response must steer toward signed angle error");

        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing diagnostics directory");
        string path = Path.Combine(folder, "source-homing-policy.txt");
        string temp = path + ".tmp";
        File.WriteAllText(temp, "BUNNY_SOURCE_HOMING_V1\nSourceBolt\n" +
            radius.ToString(CultureInfo.InvariantCulture) + "\t" +
            retarget.ToString(CultureInfo.InvariantCulture) + "\n" +
            string.Join("\t", lookup) + "\n");
        File.Move(temp, path, true);
        File.AppendAllText(diagnosticsPath,
            "\nHOMING_POLICY_OK samples=129 radius=" +
            radius.ToString(CultureInfo.InvariantCulture) + " retarget=" +
            retarget.ToString(CultureInfo.InvariantCulture));
    }

    private static void WriteRuntimeDefaults(object[] projectiles, object[] weapons,
        object[] buffs, object[] potions, object[] accessories,
        object[] players, object[] npcs, object[] prefixes, object[] basicItems,
        object[] tiles, object[] walls, object[] dusts, Type[] rarityTypes, string diagnosticsPath)
    {
        static T Value<T>(object instance, string name) =>
            (T)(instance.GetType().GetProperty(name)?.GetValue(instance)
                ?? throw new InvalidOperationException("Missing C# property: " + name));

        static void CheckName(string name)
        {
            if (name.Length == 0 || name.Length > 72)
                throw new InvalidOperationException("Invalid content name length");
            foreach (char ch in name)
                if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                      (ch >= '0' && ch <= '9') || ch == '_'))
                    throw new InvalidOperationException("Invalid content name");
        }

        var names = new HashSet<string>(StringComparer.Ordinal);
        var questFishNames = new HashSet<string>(StringComparer.Ordinal);
        var projectileNames = new HashSet<string>(StringComparer.Ordinal);
        var buffNames = new HashSet<string>(StringComparer.Ordinal);
        var lines = new List<string>(projectiles.Length + weapons.Length + buffs.Length +
            potions.Length + accessories.Length + players.Length + npcs.Length + prefixes.Length + rarityTypes.Length + tiles.Length + dusts.Length + 1)
        {
            "BUNNY_MANAGED_DEFAULTS_V2"
        };
        foreach (object wall in walls) {
            string name = wall.GetType().Name;
            CheckName(name);
            if (!names.Add("WALL:" + name)) throw new InvalidOperationException("Duplicate BunnyWall: " + name);
            lines.Add(Value<string>(wall, "NativeDefinition"));
        }
        foreach (object tile in tiles)
        {
            string name = tile.GetType().Name;
            CheckName(name);
            if (!names.Add("TILE:" + name))
                throw new InvalidOperationException("Duplicate BunnyTile: " + name);
            lines.Add(TileWriter.Write(tile));
        }
        foreach (object dust in dusts)
        {
            Type type = dust.GetType();
            string name = type.Name;
            CheckName(name);
            if (!names.Add("DUST:" + name))
                throw new InvalidOperationException("Duplicate BunnyDust: " + name);
            string texture = Value<string>(dust, "Texture");
            int updateType = Value<int>(dust, "UpdateType");
            if (updateType < -1 || updateType >= 332 ||
                texture.Length > 240 || texture.Contains((char)92) ||
                texture.Contains("..") || texture.Contains('|') ||
                texture.Contains(':') || texture.StartsWith('/'))
                throw new InvalidOperationException("Invalid BunnyDust defaults: " + name);
            int hooks = DustHookMask(type);
            lines.Add(string.Join("\t", "DUST", name,
                updateType.ToString(CultureInfo.InvariantCulture),
                hooks.ToString(CultureInfo.InvariantCulture),
                texture.Length == 0 ? "-" : texture));
        }
        foreach (object item in basicItems)
        {
            string name=item.GetType().Name;
            CheckName(name);
            if(!names.Add("ITEM:" + name)) throw new InvalidOperationException("Duplicate BunnyItem: "+name);
            lines.Add(BasicItemWriter.Write(item));
            if (Value<bool>(item, "QuestFish"))
            {
                questFishNames.Add(name);
                if (Value<int>(item, "MaxStack") != 1)
                    throw new InvalidOperationException("Quest fish must have MaxStack=1: " + name);
                bool pre = Value<bool>(item, "QuestAvailableInPreHardmode");
                bool hard = Value<bool>(item, "QuestAvailableInHardmode");
                if (!pre && !hard)
                    throw new InvalidOperationException("Quest fish cannot be unavailable in both world phases: " + name);
                string description = Value<string>(item, "QuestDescription") ?? "";
                string location = Value<string>(item, "QuestCatchLocation") ?? "";
                foreach (string value in new[] { description, location })
                {
                    if (System.Text.Encoding.UTF8.GetByteCount(value) > 512 ||
                        value.Any(c => c == '\0' || c == '\r' || c == '\t' || c == (char)127))
                        throw new InvalidOperationException("Invalid angler quest chat text: " + name);
                }
                static string EncodeQuestText(string value) =>
                    value.Length == 0 ? "-" : Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes(value));
                int rules = Convert.ToInt32(Value<object>(item, "QuestCatchRules"), CultureInfo.InvariantCulture);
                if ((rules & ~3) != 0)
                    throw new InvalidOperationException("Unknown quest catch requirements: " + name);
                lines.Add(string.Join("\t", "QUEST_FISH", name,
                    pre ? "1" : "0", hard ? "1" : "0",
                    EncodeQuestText(description), EncodeQuestText(location),
                    rules.ToString(CultureInfo.InvariantCulture)));
                int callbackMask=(item.GetType().GetProperty("QuestAvailableWhen")?.GetValue(item) is Delegate?1:0) |
                    (item.GetType().GetProperty("QuestCanCatchWhen")?.GetValue(item) is Delegate?2:0);
                if(callbackMask!=0)
                    lines.Add(string.Join("\t","QUEST_CALLBACK",name,callbackMask.ToString(CultureInfo.InvariantCulture)));
                var descriptions = Value<Dictionary<string,string>>(item, "QuestDescriptionTranslations");
                var locations = Value<Dictionary<string,string>>(item, "QuestCatchLocationTranslations");
                var cultures = descriptions.Keys.Concat(locations.Keys)
                    .Distinct(StringComparer.Ordinal).OrderBy(x => x, StringComparer.Ordinal).ToArray();
                if (cultures.Length > 8)
                    throw new InvalidOperationException("Too many quest dialogue cultures: " + name);
                foreach (string culture in cultures)
                {
                    if (culture.Length < 2 || culture.Length > 20 ||
                        culture.Any(ch => !((ch >= 'A' && ch <= 'Z') ||
                                            (ch >= 'a' && ch <= 'z') ||
                                            (ch >= '0' && ch <= '9') || ch == '-')))
                        throw new InvalidOperationException("Invalid quest dialogue culture: " + name);
                    string translatedDescription = descriptions.TryGetValue(culture, out var d) ? d : "";
                    string translatedLocation = locations.TryGetValue(culture, out var l) ? l : "";
                    if (translatedDescription.Length == 0 && translatedLocation.Length == 0)
                        throw new InvalidOperationException("Empty quest dialogue translation: " + name);
                    foreach (string value in new[] { translatedDescription, translatedLocation })
                        if (System.Text.Encoding.UTF8.GetByteCount(value) > 512 ||
                            value.Any(ch => ch == '\0' || ch == '\r' || ch == '\t' || ch == (char)127))
                            throw new InvalidOperationException("Invalid translated quest dialogue: " + name);
                    lines.Add(string.Join("\t", "QUEST_LOCALE", name, culture,
                        EncodeQuestText(translatedDescription), EncodeQuestText(translatedLocation)));
                }
            }
        }
        foreach (object projectile in projectiles)
        {
            string name = projectile.GetType().Name;
            CheckName(name);
            if (!names.Add("PROJECTILE:" + name))
                throw new InvalidOperationException("Duplicate content class: " + name);
            projectileNames.Add(name);
            bool minion = Value<bool>(projectile, "Minion");
            bool contactDamage = Value<bool>(projectile, "MinionContactDamage");
            object? motion = projectile.GetType().GetProperty("MinionMovement")!.GetValue(projectile);
            if(motion!=null&&projectile.GetType().Assembly.GetType(projectile.GetType().FullName+"Motion")!=null)
                throw new InvalidOperationException("[CSharp][ERROR][ProjectilePolicy] Class="+projectile.GetType().FullName+" API=MinionMovement Reason=ConflictingMotionCurve Result=Rejected");
            if(motion!=null&&Value<int>(projectile,"AiStyle")!=0)
                throw new InvalidOperationException("[CSharp][ERROR][ProjectilePolicy] Class="+projectile.GetType().FullName+" API=MinionMovement Expected=AiStyle0 Reason=ConflictingVanillaAI Result=Rejected");
            if(motion!=null && projectile.GetType().GetMethod("AI")!.DeclaringType!.FullName!="BunnyLoader.Source.BunnyProjectile")
                throw new InvalidOperationException("[CSharp][ERROR][ProjectilePolicy] Class="+projectile.GetType().FullName+" API=MinionMovement Stage=Validate Reason=ConflictingMovementControllers Expected=NativeMovementOrOverrideAI Result=Rejected");
            if(motion!=null&&!minion)throw new InvalidOperationException("MinionMovement requires Minion=true: "+name);
            if (contactDamage && !minion)
                throw new InvalidOperationException("MinionContactDamage requires Minion=true: " + name);
            int frames = Value<int>(projectile, "Frames");
            if (frames < 1 || frames > 32)
                throw new InvalidOperationException("Invalid C# projectile sprite frame count: " + name);
            int frameTicks=Value<int>(projectile,"AnimationFrameTicks");
            int scanTicks=Value<int>(projectile,"MinionScanTicks");
            float range=Value<float>(projectile,"MinionTargetRange");
            bool customPolicy=frameTicks!=6||scanTicks!=8||range!=700f||motion!=null;
            if(frameTicks<1||frameTicks>120||scanTicks<1||scanTicks>120||!float.IsFinite(range)||range<1||range>4000)
                throw new InvalidOperationException("[CSharp][ERROR][ProjectilePolicy] Class="+projectile.GetType().FullName+" Stage=SetDefaults Expected=AnimationFrameTicks1..120,MinionScanTicks1..120,MinionTargetRange1..4000 Actual="+frameTicks+","+scanTicks+","+range+" Reason=OutOfRange");
            float minionSlots = Value<float>(projectile, "MinionSlots");
            bool tileCollide = Value<bool>(projectile, "TileCollide");
            bool ignoreWater = Value<bool>(projectile, "IgnoreWater");
            if (!float.IsFinite(minionSlots) || (minion && (minionSlots <= 0 || minionSlots > 10)) ||
                (!minion && minionSlots != 0))
                throw new InvalidOperationException("Invalid C# minion slot contract: " + name);
            string entry = string.Join("\t", "PROJECTILE", name,
                Value<int>(projectile, "Width").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "Height").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "AiStyle").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "Penetrate").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "TimeLeft").ToString(CultureInfo.InvariantCulture),
                Value<bool>(projectile, "Friendly") ? "1" : "0");
            if (minion || !tileCollide || !ignoreWater || frames > 1 || customPolicy || contactDamage)
            {
                entry += "\t" + (minion ? "1" : "0") + "\t" +
                    minionSlots.ToString("R", CultureInfo.InvariantCulture) + "\t" +
                    (tileCollide ? "1" : "0") + "\t" + (ignoreWater ? "1" : "0");
                if (frames > 1 || customPolicy || contactDamage) entry += "\t" + frames.ToString(CultureInfo.InvariantCulture);
                if(customPolicy || contactDamage)
                    entry+="\t"+frameTicks.ToString(CultureInfo.InvariantCulture)+"\t"+scanTicks.ToString(CultureInfo.InvariantCulture)+"\t"+range.ToString("R",CultureInfo.InvariantCulture);
                if (contactDamage || motion!=null) entry += "\t"+(contactDamage?"1":"0");
                if(motion!=null)entry+="\t"+string.Join(",",new[]{"FollowSpeed","AttackSpeed","CatchUpSpeed","FollowInertia","AttackInertia","FollowOffsetX","FollowOffsetY","CatchUpDistance","IdleDistance","IdleDamping","CatchUpInertia","TeleportDistance","FormationSpacing","AttackStopDistance","OwnerLeashDistance","RotationFactor","Separation"}.Select(property=>Value<float>(motion,property).ToString("R",CultureInfo.InvariantCulture)));
            }
            lines.Add(entry);
        }
        foreach (object weapon in weapons)
        {
            string name = weapon.GetType().Name;
            string shootName = Value<string>(weapon, "Shoot");
            CheckName(name);
            CheckName(shootName);
            if (!names.Add("ITEM:" + name))
                throw new InvalidOperationException("Duplicate content class: " + name);
            if (!projectileNames.Contains(shootName))
                throw new InvalidOperationException("Weapon target projectile not found: " + shootName);
            bool summon = Value<bool>(weapon, "Summon");
            string minionBuff = Value<string>(weapon, "MinionBuff");
            int minionBuffTime = Value<int>(weapon, "MinionBuffTime");
            if ((summon && (string.IsNullOrEmpty(minionBuff) || minionBuffTime < 1 || minionBuffTime > 36000)) ||
                (!summon && !string.IsNullOrEmpty(minionBuff)))
                throw new InvalidOperationException("Invalid C# summon weapon contract: " + name);
            if (summon) CheckName(minionBuff);
            string weaponEntry = string.Join("\t", "WEAPON", name,
                Value<int>(weapon, "Damage").ToString(CultureInfo.InvariantCulture),
                Value<int>(weapon, "UseTime").ToString(CultureInfo.InvariantCulture),
                Value<float>(weapon, "ShootSpeed").ToString("R", CultureInfo.InvariantCulture),
                shootName);
            if (summon)
                weaponEntry += "\t1\t" + minionBuff + "\t" + minionBuffTime.ToString(CultureInfo.InvariantCulture);
            lines.Add(weaponEntry);
        }
        foreach (object buff in buffs)
        {
            string name = buff.GetType().Name;
            CheckName(name);
            if (!names.Add("BUFF:" + name))
                throw new InvalidOperationException("Duplicate content class: " + name);
            buffNames.Add(name);
            int defense = Value<int>(buff, "DefenseBonus");
            if (defense < -50 || defense > 100)
                throw new InvalidOperationException("Invalid C# buff defense bonus");
            string minion = Value<string>(buff, "MinionProjectile") ?? "";
            if (minion.Length > 0)
            {
                CheckName(minion);
                if (!projectileNames.Contains(minion))
                    throw new InvalidOperationException("Buff minion projectile not found: " + minion);
            }
            string description=Value<string>(buff,"Description") ?? "";
            if(System.Text.Encoding.UTF8.GetByteCount(description)>512 || description.Any(c=>char.IsControl(c)&&c!='\n'))
                throw new InvalidOperationException("[CSharp][ERROR][Buff] Class="+buff.GetType().FullName+" Field=Description Stage=SetDefaults Expected=Utf8UpTo512Bytes Reason=InvalidDescription");
            description=description.Replace("{0}",defense.ToString(CultureInfo.InvariantCulture));
            int multiplier=Value<int>(buff,"DefenseMultiplierPercent");
            if(multiplier<0||multiplier>100)throw new InvalidOperationException("[CSharp][ERROR][Buff] Class="+name+" Field=DefenseMultiplierPercent Expected=0..100 Actual="+multiplier+" Reason=InvalidDefenseMultiplier");
            int flags=(Value<bool>(buff,"IsDebuff")?1:0)|(Value<bool>(buff,"NoSave")?2:0)|(Value<bool>(buff,"NoTimeDisplay")?4:0)|(Value<bool>(buff,"NurseCannotRemove")?8:0);
            // Keep the old row for unchanged mods; extended rows carry values executed by MonoVM.
            if(minion.Length==0 && flags==0 && description.Length==0 && multiplier==100)
                lines.Add(string.Join("\t","BUFF",name,defense.ToString(CultureInfo.InvariantCulture)));
            else lines.Add(string.Join("\t", "BUFF", name,
                defense.ToString(CultureInfo.InvariantCulture), minion.Length > 0 ? minion : "-",
                flags.ToString(CultureInfo.InvariantCulture),description.Length==0?"-":Convert.ToHexString(System.Text.Encoding.UTF8.GetBytes(description)),
                multiplier.ToString(CultureInfo.InvariantCulture)));
        }
        foreach (object potion in potions)
        {
            string name = potion.GetType().Name;
            string buffName = Value<string>(potion, "Buff");
            CheckName(name);
            CheckName(buffName);
            if (!names.Add("ITEM:" + name) || !buffNames.Contains(buffName))
                throw new InvalidOperationException("C# buff potion target missing or duplicated");
            int duration = Value<int>(potion, "Duration");
            if (duration < 1 || duration > 216000)
                throw new InvalidOperationException("Invalid C# buff potion duration");
            lines.Add(string.Join("\t", "POTION", name,
                duration.ToString(CultureInfo.InvariantCulture), buffName));
        }
        var accessoryNames = new HashSet<string>(StringComparer.Ordinal);
        foreach (object accessory in accessories)
        {
            string name = accessory.GetType().Name;
            CheckName(name);
            if (!names.Add("ITEM:" + name)) throw new InvalidOperationException("Duplicated C# accessory");
            accessoryNames.Add(name);
            int defense = Value<int>(accessory, "DefenseBonus");
            string tooltip = Value<string>(accessory, "Tooltip");
            if (defense < 0 || defense > 100 || tooltip.Length > 180 ||
                tooltip.Any(char.IsControl))
                throw new InvalidOperationException("Invalid C# accessory defaults or tooltip");
            int visualMode = Convert.ToInt32(Value<object>(accessory, "VisualMode"), CultureInfo.InvariantCulture);
            string visualTexture = Value<string>(accessory, "VisualTexture");
            float visualScale = Value<float>(accessory, "VisualScale");
            float visualOpacity = Value<float>(accessory, "VisualOpacity");
            uint visualColor = Value<uint>(accessory, "VisualColor");
            if (visualMode < 0 || visualMode > 3 ||
                (visualMode != 0 && (visualTexture.Length == 0 || visualTexture.Length > 180 ||
                                     !visualTexture.EndsWith(".png", StringComparison.OrdinalIgnoreCase))) ||
                visualTexture.Contains("..", StringComparison.Ordinal) ||
                visualTexture.Contains('|') || visualTexture.IndexOf((char)92) >= 0 ||
                visualTexture.Any(char.IsControl) ||
                !float.IsFinite(visualScale) || visualScale < 0.25f || visualScale > 3f ||
                !float.IsFinite(visualOpacity) || visualOpacity < 0f || visualOpacity > 1f)
                throw new InvalidOperationException("Invalid C# accessory visual policy");
            string[] entry = {"ACCESSORY", name, defense.ToString(CultureInfo.InvariantCulture),
                tooltip.Length == 0 ? "-" : tooltip};
            if (visualMode == 0)
                lines.Add(string.Join("\t", entry));
            else
                lines.Add(string.Join("\t", entry) + "\t" +
                    visualMode.ToString(CultureInfo.InvariantCulture) + "\t" +
                    visualTexture + "\t" +
                    visualScale.ToString("R", CultureInfo.InvariantCulture) + "\t" +
                    visualOpacity.ToString("R", CultureInfo.InvariantCulture) + "\t" +
                    visualColor.ToString(CultureInfo.InvariantCulture));
        }
        foreach (object player in players)
        {
            string name = player.GetType().Name;
            string accessoryName = Value<string>(player, "Accessory");
            CheckName(name);
            bool globalPlayer = accessoryName.Length == 0;
            if (!globalPlayer) CheckName(accessoryName);
            if (!names.Add("PLAYER:" + name) || (!globalPlayer && !accessoryNames.Contains(accessoryName)))
                throw new InvalidOperationException("BunnyPlayer linked accessory not found");
            var reset = player.GetType().GetMethod("ResetEffects")
                ?? throw new InvalidOperationException("BunnyPlayer.ResetEffects missing");
            Type playerType = player.GetType();
            var update = playerType.GetMethod("UpdateEquips")
                ?? throw new InvalidOperationException("BunnyPlayer.UpdateEquips missing");
            var entering = playerType.GetMethod("OnEnterWorld")
                ?? throw new InvalidOperationException("BunnyPlayer.OnEnterWorld missing");
            var respawning = playerType.GetMethod("OnRespawn")
                ?? throw new InvalidOperationException("BunnyPlayer.OnRespawn missing");
            var movement = playerType.GetMethod("UpdateMovement")
                ?? throw new InvalidOperationException("BunnyPlayer.UpdateMovement missing");
            var frames = playerType.GetMethod("FrameEffects")
                ?? throw new InvalidOperationException("BunnyPlayer.FrameEffects missing");
            var pre = playerType.GetMethod("PreUpdate")
                ?? throw new InvalidOperationException("BunnyPlayer.PreUpdate missing");
            var post = playerType.GetMethod("PostUpdate")
                ?? throw new InvalidOperationException("BunnyPlayer.PostUpdate missing");
            var dead = playerType.GetMethod("UpdateDead")
                ?? throw new InvalidOperationException("BunnyPlayer.UpdateDead missing");
            var life = playerType.GetMethod("UpdateLifeRegen")
                ?? throw new InvalidOperationException("BunnyPlayer.UpdateLifeRegen missing");
            var mana = playerType.GetMethod("UpdateManaRegen")
                ?? throw new InvalidOperationException("BunnyPlayer.UpdateManaRegen missing");
            var modifyHurt = playerType.GetMethod("ModifyHurt")
                ?? throw new InvalidOperationException("BunnyPlayer.ModifyHurt missing");
            var modifyNpc = playerType.GetMethod("ModifyHitByNPC")
                ?? throw new InvalidOperationException("BunnyPlayer.ModifyHitByNPC missing");
            var modifyProjectile = playerType.GetMethod("ModifyHitByProjectile")
                ?? throw new InvalidOperationException("BunnyPlayer.ModifyHitByProjectile missing");
            var onHurt = playerType.GetMethod("OnHurt")
                ?? throw new InvalidOperationException("BunnyPlayer.OnHurt missing");
            var onHitByNpc = playerType.GetMethod("OnHitByNPC")
                ?? throw new InvalidOperationException("BunnyPlayer.OnHitByNPC missing");
            var onHitByProjectile = playerType.GetMethod("OnHitByProjectile")
                ?? throw new InvalidOperationException("BunnyPlayer.OnHitByProjectile missing");
            var freeDodge = playerType.GetMethod("FreeDodge")
                ?? throw new InvalidOperationException("BunnyPlayer.FreeDodge missing");
            if (modifyHurt.ReturnType != typeof(void) ||
                modifyNpc.ReturnType != typeof(void) ||
                modifyProjectile.ReturnType != typeof(void) ||
                !modifyNpc.GetParameters().Select(p => p.ParameterType.FullName)
                    .SequenceEqual(new[] { "System.Boolean", "System.Int32", "BunnyLoader.Source.BunnyHurtModifiers" }) ||
                !modifyProjectile.GetParameters().Select(p => p.ParameterType.FullName)
                    .SequenceEqual(new[] { "System.Boolean", "System.Int32", "BunnyLoader.Source.BunnyHurtModifiers" }) ||
                onHurt.ReturnType != typeof(void) ||
                freeDodge.ReturnType != typeof(bool) ||
                onHitByNpc.ReturnType != typeof(void) ||
                onHitByProjectile.ReturnType != typeof(void) ||
                !onHitByNpc.GetParameters().Select(p => p.ParameterType.FullName)
                    .SequenceEqual(new[] { "System.Boolean", "System.Int32", "BunnyLoader.Source.BunnyHurtInfo" }) ||
                !onHitByProjectile.GetParameters().Select(p => p.ParameterType.FullName)
                    .SequenceEqual(new[] { "System.Boolean", "System.Int32", "BunnyLoader.Source.BunnyHurtInfo" }) ||
                !freeDodge.GetParameters().Select(p => p.ParameterType.FullName)
                    .SequenceEqual(new[] { "System.Boolean", "BunnyLoader.Source.BunnyHurtModifiers" }) ||
                !modifyHurt.GetParameters().Select(p => p.ParameterType.FullName)
                    .SequenceEqual(new[] { "System.Boolean", "BunnyLoader.Source.BunnyHurtModifiers" }) ||
                !onHurt.GetParameters().Select(p => p.ParameterType.FullName)
                    .SequenceEqual(new[] { "System.Boolean", "BunnyLoader.Source.BunnyHurtInfo" }))
                throw new InvalidOperationException("BunnyPlayer hurt callback ABI mismatch");
            reset.Invoke(player, null);
            update.Invoke(player, new object[] { false });
            int noMove = (int)(movement.Invoke(player, new object[] { false }) ?? -1);
            bool noFrames = (bool)(frames.Invoke(player, new object[] { false }) ?? true);
            int noLife = (int)(life.Invoke(player, new object[] { false }) ?? -1);
            int noMana = (int)(mana.Invoke(player, new object[] { false }) ?? -1);
            if (Value<int>(player, "DefenseBonus") != 0 ||
                Value<int>(player, "MoveSpeedPercent") != 0 ||
                noMove != 0 || noFrames || noLife != 0 || noMana != 0)
                throw new InvalidOperationException("BunnyPlayer modified effects while unequipped");
            reset.Invoke(player, null);
            update.Invoke(player, new object[] { true });
            int defense = Value<int>(player, "DefenseBonus");
            int moveSpeed = Value<int>(player, "MoveSpeedPercent");
            int acceleration = (int)(movement.Invoke(player, new object[] { true }) ?? -1);
            bool shadow = (bool)(frames.Invoke(player, new object[] { true }) ?? false);
            string welcome = (string)(entering.Invoke(player, null) ?? "");
            string respawn = (string)(respawning.Invoke(player, null) ?? "");
            bool beforeTick = (bool)(pre.Invoke(player, null) ?? false);
            bool afterTick = (bool)(post.Invoke(player, null) ?? false);
            string deadMessage = (string)(dead.Invoke(player, null) ?? "");
            int lifeRegen = (int)(life.Invoke(player, new object[] { true }) ?? -1);
            int manaRegen = (int)(mana.Invoke(player, new object[] { true }) ?? -1);
            if (defense < 0 || defense > 100 || moveSpeed < 0 || moveSpeed > 50 ||
                acceleration < 0 || acceleration > 30 || welcome.Length > 100 ||
                respawn.Length > 100 || deadMessage.Length > 100 ||
                lifeRegen < 0 || lifeRegen > 30 || manaRegen < 0 || manaRegen > 30 ||
                welcome.Any(c => char.IsControl(c) || c > 127) ||
                respawn.Any(c => char.IsControl(c) || c > 127) ||
                deadMessage.Any(c => char.IsControl(c) || c > 127) ||
                (globalPlayer && (defense != 0 || moveSpeed != 0 || acceleration != 0 ||
                                  shadow || lifeRegen != 0 || manaRegen != 0)))
                throw new InvalidOperationException("BunnyPlayer event policy outside supported bounds");
            reset.Invoke(player, null);
            if (Value<int>(player, "DefenseBonus") != 0 ||
                Value<int>(player, "MoveSpeedPercent") != 0)
                throw new InvalidOperationException("BunnyPlayer.ResetEffects did not reset equipped effects");
            lines.Add(string.Join("\t", "PLAYER", name,
                globalPlayer ? "-" : accessoryName,
                defense.ToString(CultureInfo.InvariantCulture),
                moveSpeed.ToString(CultureInfo.InvariantCulture),
                acceleration.ToString(CultureInfo.InvariantCulture),
                shadow ? "1" : "0",
                welcome.Length == 0 ? "-" : welcome,
                respawn.Length == 0 ? "-" : respawn,
                beforeTick ? "1" : "0",
                afterTick ? "1" : "0",
                deadMessage.Length == 0 ? "-" : deadMessage,
                lifeRegen.ToString(CultureInfo.InvariantCulture),
                manaRegen.ToString(CultureInfo.InvariantCulture)));
            string questFishName = Value<string>(player, "FishingQuestFishName") ?? "";
            int catchRules = Convert.ToInt32(Value<object>(player, "FishingCatchRules"), CultureInfo.InvariantCulture);
            if (questFishName.Length == 0 && catchRules != 0)
                throw new InvalidOperationException("FishingCatchRules requires FishingQuestFishName: " + name);
            if (questFishName.Length != 0)
            {
                CheckName(questFishName);
                if (!questFishNames.Contains(questFishName) || (catchRules & ~3) != 0)
                    throw new InvalidOperationException("FishingQuestFish must reference an owned quest BunnyItem: " + name);
                lines.Add(string.Join("\t", "QUEST_CATCH", name, questFishName,
                    catchRules.ToString(CultureInfo.InvariantCulture)));
            }
        }
        foreach (object npc in npcs)
        {
            string name = npc.GetType().Name;
            CheckName(name);
            if (!names.Add("NPC:" + name))
                throw new InvalidOperationException("Duplicate NPC C# class: " + name);
            int width = Value<int>(npc, "Width");
            int height = Value<int>(npc, "Height");
            int damage = Value<int>(npc, "Damage");
            int defense = Value<int>(npc, "Defense");
            int life = Value<int>(npc, "LifeMax");
            int ai = Value<int>(npc, "AiStyle");
            int animation = Value<int>(npc, "AnimationType");
            int frames = Value<int>(npc, "Frames");
            if (width < 8 || width > 256 || height < 8 || height > 256 ||
                damage < 0 || damage > 500 || defense < 0 || defense > 200 ||
                life < 1 || life > 100000 || ai != 1 || animation != 1 || frames != 2)
                throw new InvalidOperationException("BunnyNPC only supports validated slime AI defaults");
            lines.Add(string.Join("\t", "NPC", name,
                width.ToString(CultureInfo.InvariantCulture),
                height.ToString(CultureInfo.InvariantCulture),
                damage.ToString(CultureInfo.InvariantCulture),
                defense.ToString(CultureInfo.InvariantCulture),
                life.ToString(CultureInfo.InvariantCulture),
                ai.ToString(CultureInfo.InvariantCulture),
                animation.ToString(CultureInfo.InvariantCulture),
                frames.ToString(CultureInfo.InvariantCulture)));
        }

        foreach (Type rarity in rarityTypes)
        {
            string name = rarity.Name;
            CheckName(name);
            if (!names.Add("RARITY:" + name))
                throw new InvalidOperationException("Duplicate BunnyRarity: " + name);
            lines.Add("RARITY\t" + name);
        }
        foreach (object prefix in prefixes)
        {
            Type type = prefix.GetType();
            string name = type.Name;
            CheckName(name);
            if (!names.Add("PREFIX:" + name))
                throw new InvalidOperationException("Duplicate managed prefix: " + name);
            int category = Convert.ToInt32(Value<object>(prefix, "Category"), CultureInfo.InvariantCulture);
            float weight = Value<float>(prefix, "RollWeight");
            if (category < 0 || category > 5 || !float.IsFinite(weight) ||
                weight <= 0f || weight > 20f || weight != MathF.Truncate(weight))
                throw new InvalidOperationException("Unsupported managed prefix category or weight: " + name);
            MethodInfo setStats = type.GetMethod("SetStats",
                BindingFlags.Public | BindingFlags.Instance)
                ?? throw new InvalidOperationException("Missing BunnyPrefix.SetStats");
            Type statsType = setStats.GetParameters().Single().ParameterType.GetElementType()
                ?? throw new InvalidOperationException("PrefixStats must be passed by reference");
            object stats = statsType.GetProperty("Identity", BindingFlags.Public | BindingFlags.Static)!
                .GetValue(null) ?? throw new InvalidOperationException("PrefixStats.Identity missing");
            object?[] statsArgs = { stats };
            setStats.Invoke(prefix, statsArgs);
            stats = statsArgs[0] ?? throw new InvalidOperationException("PrefixStats result missing");
            float[] multipliers = {
                Value<float>(stats, "DamageMultiplier"),
                Value<float>(stats, "KnockbackMultiplier"),
                Value<float>(stats, "UseTimeMultiplier"),
                Value<float>(stats, "ScaleMultiplier"),
                Value<float>(stats, "ShootSpeedMultiplier"),
                Value<float>(stats, "ManaMultiplier")
            };
            foreach (float multiplier in multipliers)
                if (!float.IsFinite(multiplier) || multiplier < 0.1f || multiplier > 10f)
                    throw new InvalidOperationException("Invalid C# prefix multiplier: " + name);
            int crit = Value<int>(stats, "CriticalBonus");
            int tag = Value<int>(stats, "TagDamageBonus");
            int penetration = Value<int>(stats, "ArmorPenetrationBonus");
            if (crit < -100 || crit > 100 || tag < -100 || tag > 100 ||
                penetration < -100 || penetration > 100)
                throw new InvalidOperationException("Invalid C# prefix bonus: " + name);
            float value = multipliers[0] * (2f - multipliers[2]) *
                (2f - multipliers[5]) * multipliers[3] * multipliers[1] *
                multipliers[4] * (1f + crit * 0.02f) *
                (1f + penetration * 0.015f) * (1f + tag * 0.03f);
            object?[] valueArgs = { value };
            type.GetMethod("ModifyValue", BindingFlags.Public | BindingFlags.Instance)!
                .Invoke(prefix, valueArgs);
            value = (float)(valueArgs[0] ?? throw new InvalidOperationException("Prefix value missing"));
            object?[] defenseArgs = { 0 };
            type.GetMethod("ApplyAccessoryEffects", BindingFlags.Public | BindingFlags.Instance)!
                .Invoke(prefix, defenseArgs);
            int defense = (int)(defenseArgs[0] ?? throw new InvalidOperationException("Prefix defense missing"));
            if (!float.IsFinite(value) || value < 0.05f || value > 100f ||
                defense < -100 || defense > 100 ||
                (category != 5 && defense != 0))
                throw new InvalidOperationException("Invalid C# prefix value or defense: " + name);
            var fields = new List<string> { "PREFIX", name,
                category.ToString(CultureInfo.InvariantCulture),
                weight.ToString("R", CultureInfo.InvariantCulture) };
            foreach (float multiplier in multipliers)
                fields.Add(multiplier.ToString("R", CultureInfo.InvariantCulture));
            fields.Add(crit.ToString(CultureInfo.InvariantCulture));
            fields.Add(tag.ToString(CultureInfo.InvariantCulture));
            fields.Add(penetration.ToString(CultureInfo.InvariantCulture));
            fields.Add(value.ToString("R", CultureInfo.InvariantCulture));
            fields.Add(defense.ToString(CultureInfo.InvariantCulture));
            string display = Value<string>(prefix, "DisplayName");
            if (string.IsNullOrEmpty(display)) display = name;
            if (display.Length > 80 || display.Any(char.IsControl))
                throw new InvalidOperationException("Invalid C# prefix name");
            fields.Add(display);
            lines.Add(string.Join("\t", fields));
        }
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing managed diagnostics directory");
        string destination = Path.Combine(folder, "source-runtime-values.txt");
        string temporary = destination + ".tmp";
        // Specialized items share the Bunny recipe loader; primary V2 rows stay unchanged.
        foreach(var owner in weapons.Select(o=>(Value:o,Kind:"WEAPON"))
            .Concat(accessories.Select(o=>(Value:o,Kind:"ACCESSORY")))
            .Concat(potions.Select(o=>(Value:o,Kind:"POTION"))))
        {
            var type=owner.Value.GetType();
            type.GetMethod("AddRecipes",Type.EmptyTypes)!.Invoke(owner.Value,null);
            var recipes=(IReadOnlyList<string>)type.GetProperty("RecipeDefinitions")!.GetValue(owner.Value)!;
            if(recipes.Count==0)continue;
            if(recipes.Count>16)throw new InvalidOperationException(type.FullName+": recipe capacity exceeded");
            lines.Add(string.Join("\t","RECIPES",type.Name,owner.Kind,string.Join(";",recipes)));
        }
        foreach (object item in basicItems.Concat(weapons).Concat(potions).Concat(accessories))
        {
            string owner = item.GetType().Name;
            string rarity = Value<string>(item, "ModRarityClassName");
            if (rarity.Length == 0) continue;
            CheckName(owner);
            CheckName(rarity);
            Type[] matches = rarityTypes.Where(t => t.Name == rarity).ToArray();
            if (matches.Length != 1)
                throw new InvalidOperationException("[CSharp][ERROR][RarityLoader] Class=" +
                    owner + " API=SetRarity Stage=Compile Reason=RarityNotRegisteredInSameAssembly");
            lines.Add("ITEM_RARITY\t" + owner + "\t" + rarity);
        }
        foreach(var item in basicItems.Concat(weapons))
        {
            int shootMask=Value<int>(item,"ShootCallbackMask");
            if(shootMask!=0)lines.Add("SHOOT\t"+item.GetType().Name+"\t"+shootMask.ToString(CultureInfo.InvariantCulture));
            object? sound=item.GetType().GetProperty("CustomUseSound")!.GetValue(item);
            if(sound!=null)lines.Add("USE_SOUND\t"+item.GetType().Name+"\t"+Value<string>(sound,"Definition"));
            object? holdout=item.GetType().GetProperty("Holdout")!.GetValue(item);
            if(holdout!=null)lines.Add(string.Join("\t","HOLDOUT",item.GetType().Name,
                Value<float>(holdout,"X").ToString("R",CultureInfo.InvariantCulture),Value<float>(holdout,"Y").ToString("R",CultureInfo.InvariantCulture)));
        }
        // A single public typed bridge serves all item families and projectiles.
        foreach(var definition in basicItems.Concat(weapons).Concat(potions).Concat(accessories)
            .Select(value=>(Value:value,Kind:"NATIVE_ITEM"))
            .Concat(projectiles.Select(value=>(Value:value,Kind:"NATIVE_PROJECTILE"))))
        {
            string native=Value<string>(definition.Value,"NativeDefaultsDefinition");
            if(native.Length!=0)lines.Add(string.Join("\t",definition.Kind,definition.Value.GetType().Name,native));
        }
        File.WriteAllText(temporary, string.Join("\n", lines) + "\n");
        File.Move(temporary, destination, true);
        File.AppendAllText(diagnosticsPath, "\nDEFAULTS_MULTI_OK projectiles=" +
            projectiles.Length + " weapons=" + weapons.Length +
            " buffs=" + buffs.Length + " potions=" + potions.Length);
    }

}


