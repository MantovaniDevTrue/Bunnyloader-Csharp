using System;
using System.IO;
using System.Linq;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;

public sealed class ProbeKeepAlive : BunnyProjectile
{
    public override void SetDefaults()
    {
        Width = 42;
        Height = 30;
        TileCollide = true;
        TileCollisionMode = BunnyProjectileTileCollisionMode.KeepAlive;
    }
}

public sealed class ProbeBounce : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
    }
}

public sealed class ProbeCustomBounce : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
        BounceRestitution = 0.75f;
        BounceMinimumSpeed = 3f;
    }
}

public sealed class ProbeStop : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionMode = BunnyProjectileTileCollisionMode.Stop;
    }
}

public sealed class ProbeVanilla : BunnyProjectile
{
    public override void SetDefaults()
    {
        Width = 10;
        Height = 10;
    }
}

public static class Program
{
    public static void Main()
    {
        string folder = Path.Combine(Path.GetTempPath(), "bunny-tile-collision-metadata-" +
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            string diagnostics = Path.Combine(folder, "diagnostics.txt");
            int count = CompiledAssemblyProbe.Verify(typeof(Program).Assembly.Location, diagnostics);
            if (count != 5)
                throw new InvalidOperationException("Probe rejected: " + File.ReadAllText(diagnostics));
            string[] rows = File.ReadAllLines(Path.Combine(folder, "source-runtime-values.txt"));
            string[] keep = rows.Single(x => x.StartsWith("PROJECTILE\tProbeKeepAlive\t",
                StringComparison.Ordinal)).Split('\t');
            string[] vanilla = rows.Single(x => x.StartsWith("PROJECTILE\tProbeVanilla\t",
                StringComparison.Ordinal)).Split('\t');
            if (keep.Length != 19 || keep[16] != "0" || keep[17] != "-" || keep[18] != "1")
                throw new InvalidOperationException("KeepAlive wire contract did not use 19 fields");
            foreach ((string name, string expected) in new[] {
                ("ProbeBounce", "2"), ("ProbeStop", "3")
            }) {
                string[] result = rows.Single(x =>
                    x.StartsWith("PROJECTILE\t" + name + "\t", StringComparison.Ordinal)).Split('\t');
                if (result.Length != 19 || result[16] != "0" || result[17] != "-" ||
                    result[18] != expected)
                    throw new InvalidOperationException(name + " wire contract mismatched");
            }
            string[] tuned = rows.Single(x =>
                x.StartsWith("PROJECTILE\tProbeCustomBounce\t", StringComparison.Ordinal)).Split('\t');
            if (tuned.Length != 21 || tuned[18] != "2" ||
                tuned[19] != "0.75" || tuned[20] != "3")
                throw new InvalidOperationException("Custom bounce wire contract mismatched");
            if (vanilla.Length != 8)
                throw new InvalidOperationException("Vanilla projectile wire ABI changed");
            Console.WriteLine("PROJECTILE_TILE_COLLISION_WIRE_PASS");
        }
        finally
        {
            Directory.Delete(folder, true);
        }
    }
}
