using System;
using System.IO;
using System.Globalization;
using System.Reflection;
using System.Linq;
using System.Collections.Generic;

namespace BunnyLoader.SourceCompiler;

public static class CompiledAssemblyProbe
{
    public static int Verify(string assemblyPath, string diagnosticsPath)
    {
        try
        {
            if (!File.Exists(assemblyPath) || new FileInfo(assemblyPath).Length > 8 * 1024 * 1024)
                throw new InvalidOperationException("Compiled assembly missing or too large");

            Assembly assembly = Assembly.Load(File.ReadAllBytes(assemblyPath));
            var projectileTypes = assembly.GetTypes()
                .Where(type => type.BaseType?.FullName == "BunnyLoader.Source.BunnyProjectile")
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            var weaponTypes = assembly.GetTypes()
                .Where(type => type.BaseType?.FullName == "BunnyLoader.Source.BunnyWeapon")
                .OrderBy(type => type.Name, StringComparer.Ordinal).ToArray();
            if (projectileTypes.Length < 1 || projectileTypes.Length > 16 ||
                weaponTypes.Length < 1 || weaponTypes.Length > 16)
                throw new InvalidOperationException("C# content class count out of bounds");
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
            WriteRuntimeDefaults(projectiles, weapons, diagnosticsPath);
            WriteProjectileMotionPolicies(assembly, projectileTypes, diagnosticsPath);
            if (fixture)
            {
                WriteProjectileMotion(assembly, diagnosticsPath);
                WriteSparkMotion(assembly, diagnosticsPath);
                WriteHomingPolicy(assembly, diagnosticsPath);
                WriteHitNPCPolicy(primaryProjectile!, diagnosticsPath);
                WriteKillPolicy(primaryProjectile!, diagnosticsPath);
            }
            File.AppendAllText(diagnosticsPath,
                "\nASSEMBLY_EXEC_OK classes=" + (projectiles.Length + weapons.Length) +
                " projectiles=" + projectiles.Length + " weapons=" + weapons.Length +
                (fixture
                    ? " projectile=SourceBolt width=10 weapon=SourceWand damage=22 shoot=SourceBolt"
                    : " policy=generic-managed-defaults"));
            return projectiles.Length + weapons.Length;
        }
        catch (Exception exception)
        {
            File.AppendAllText(diagnosticsPath,
                "\nASSEMBLY_EXEC_FAILED " + exception.ToString()[..Math.Min(3000, exception.ToString().Length)]);
            return -1;
        }
    }

    private static object CreateContent(Type type)
    {
        object instance = Activator.CreateInstance(type)
            ?? throw new InvalidOperationException("Failed to instantiate " + type.Name);
        MethodInfo method = type.GetMethod("SetDefaults", BindingFlags.Public | BindingFlags.Instance)
            ?? throw new InvalidOperationException("Missing SetDefaults in " + type.Name);
        method.Invoke(instance, null);
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
        if (policies.Count > 16)
            throw new InvalidOperationException("More than 16 independent projectile motion curves");
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

    private static void WriteRuntimeDefaults(object[] projectiles, object[] weapons, string diagnosticsPath)
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
        var projectileNames = new HashSet<string>(StringComparer.Ordinal);
        var lines = new List<string>(projectiles.Length + weapons.Length + 1)
        {
            "BUNNY_MANAGED_DEFAULTS_V2"
        };
        foreach (object projectile in projectiles)
        {
            string name = projectile.GetType().Name;
            CheckName(name);
            if (!names.Add(name))
                throw new InvalidOperationException("Duplicate content class: " + name);
            projectileNames.Add(name);
            lines.Add(string.Join("\t", "PROJECTILE", name,
                Value<int>(projectile, "Width").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "Height").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "AiStyle").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "Penetrate").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "TimeLeft").ToString(CultureInfo.InvariantCulture),
                Value<bool>(projectile, "Friendly") ? "1" : "0"));
        }
        foreach (object weapon in weapons)
        {
            string name = weapon.GetType().Name;
            string shootName = Value<string>(weapon, "Shoot");
            CheckName(name);
            CheckName(shootName);
            if (!names.Add(name))
                throw new InvalidOperationException("Duplicate content class: " + name);
            if (!projectileNames.Contains(shootName))
                throw new InvalidOperationException("Weapon target projectile not found: " + shootName);
            lines.Add(string.Join("\t", "WEAPON", name,
                Value<int>(weapon, "Damage").ToString(CultureInfo.InvariantCulture),
                Value<int>(weapon, "UseTime").ToString(CultureInfo.InvariantCulture),
                Value<float>(weapon, "ShootSpeed").ToString("R", CultureInfo.InvariantCulture),
                shootName));
        }
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing managed diagnostics directory");
        string destination = Path.Combine(folder, "source-runtime-values.txt");
        string temporary = destination + ".tmp";
        File.WriteAllText(temporary, string.Join("\n", lines) + "\n");
        File.Move(temporary, destination, true);
        File.AppendAllText(diagnosticsPath, "\nDEFAULTS_MULTI_OK projectiles=" +
            projectiles.Length + " weapons=" + weapons.Length);
    }

}
