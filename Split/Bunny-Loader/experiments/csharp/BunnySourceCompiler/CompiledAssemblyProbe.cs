using System;
using System.IO;
using System.Globalization;
using System.Reflection;

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
            object projectile = VerifyProjectile(assembly);
            object weapon = VerifyWeapon(assembly);
            WriteRuntimeDefaults(projectile, weapon, diagnosticsPath);
            File.AppendAllText(diagnosticsPath,
                "\nASSEMBLY_EXEC_OK classes=2 projectile=SourceBolt width=10 weapon=SourceWand damage=22 shoot=SourceBolt");
            return 2;
        }
        catch (Exception exception)
        {
            File.AppendAllText(diagnosticsPath,
                "\nASSEMBLY_EXEC_FAILED " + exception.ToString()[..Math.Min(3000, exception.ToString().Length)]);
            return -1;
        }
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
    private static void WriteRuntimeDefaults(object projectile, object weapon, string diagnosticsPath)
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

        string projectileName = projectile.GetType().Name;
        string weaponName = weapon.GetType().Name;
        string shootName = Value<string>(weapon, "Shoot");
        CheckName(projectileName);
        CheckName(weaponName);
        CheckName(shootName);
        if (shootName != projectileName)
            throw new InvalidOperationException("Shoot must target compiled projectile");

        string lines = "BUNNY_MANAGED_DEFAULTS_V1\n" +
            string.Join("\t", "PROJECTILE", projectileName,
                Value<int>(projectile, "Width").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "Height").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "AiStyle").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "Penetrate").ToString(CultureInfo.InvariantCulture),
                Value<int>(projectile, "TimeLeft").ToString(CultureInfo.InvariantCulture),
                Value<bool>(projectile, "Friendly") ? "1" : "0") + "\n" +
            string.Join("\t", "WEAPON", weaponName,
                Value<int>(weapon, "Damage").ToString(CultureInfo.InvariantCulture),
                Value<int>(weapon, "UseTime").ToString(CultureInfo.InvariantCulture),
                Value<float>(weapon, "ShootSpeed").ToString("R", CultureInfo.InvariantCulture),
                shootName) + "\n";
        string folder = Path.GetDirectoryName(diagnosticsPath)
            ?? throw new InvalidOperationException("Missing managed diagnostics directory");
        string destination = Path.Combine(folder, "source-runtime-values.txt");
        string temporary = destination + ".tmp";
        File.WriteAllText(temporary, lines);
        File.Move(temporary, destination, true);
    }

}
