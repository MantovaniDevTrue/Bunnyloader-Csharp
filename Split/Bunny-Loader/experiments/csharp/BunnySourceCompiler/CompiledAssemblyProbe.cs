using System;
using System.IO;
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
            VerifyProjectile(assembly);
            VerifyWeapon(assembly);
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

    private static void VerifyProjectile(Assembly assembly)
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
    }

    private static void VerifyWeapon(Assembly assembly)
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
    }
}
