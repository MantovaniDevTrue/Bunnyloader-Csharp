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

public sealed class ProbeLimitedBounce : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
        TileCollisionSurvivalLimit = 3;
    }
}

public sealed class ProbeLimitedStop : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionMode = BunnyProjectileTileCollisionMode.Stop;
        TileCollisionSurvivalLimit = 1;
    }
}

public sealed class ProbeLimitedTunedBounce : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
        BounceRestitution = 0.5f;
        BounceMinimumSpeed = 4f;
        TileCollisionSurvivalLimit = 2;
    }
}

public sealed class ProbeSolidPlatforms : BunnyProjectile
{
    public override void SetDefaults()
    {
        TilePlatformCollision = BunnyProjectilePlatformCollision.SolidPlatforms;
    }
}

public sealed class ProbePassThroughPlatforms : BunnyProjectile
{
    public override void SetDefaults()
    {
        TilePlatformCollision = BunnyProjectilePlatformCollision.PassThroughPlatforms;
        TileCollisionMode = BunnyProjectileTileCollisionMode.KeepAlive;
    }
}

public sealed class ProbeCustomCollisionBox : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionBoxWidth = 18;
        TileCollisionBoxHeight = 12;
        TileCollisionAnchorOffsetX = 2.5f;
        TileCollisionAnchorOffsetY = -3f;
    }
}

public sealed class ProbeCollisionBoxWithPlatforms : BunnyProjectile
{
    public override void SetDefaults()
    {
        TileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
        TilePlatformCollision = BunnyProjectilePlatformCollision.SolidPlatforms;
        TileCollisionBoxWidth = 12;
        TileCollisionBoxHeight = 6;
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
            if (count != 12)
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
            foreach (var data in new[] {
                (Name:"ProbeLimitedBounce", Mode:"2", Restitution:"1", Speed:"0", Limit:"3"),
                (Name:"ProbeLimitedStop", Mode:"3", Restitution:"1", Speed:"0", Limit:"1"),
                (Name:"ProbeLimitedTunedBounce", Mode:"2", Restitution:"0.5", Speed:"4", Limit:"2")
            }) {
                string[] row = rows.Single(x =>
                    x.StartsWith("PROJECTILE\t" + data.Name + "\t", StringComparison.Ordinal)).Split('\t');
                if (row.Length != 22 || row[18] != data.Mode || row[19] != data.Restitution ||
                    row[20] != data.Speed || row[21] != data.Limit)
                    throw new InvalidOperationException(data.Name + " survival limit wire contract mismatched");
            }
            foreach(var style in new[] {
                (Name:"ProbeSolidPlatforms",Collision:"0",Platform:"1"),
                (Name:"ProbePassThroughPlatforms",Collision:"1",Platform:"2")
            }) {
                string[] row=rows.Single(x =>
                    x.StartsWith("PROJECTILE\t"+style.Name+"\t",StringComparison.Ordinal)).Split('\t');
                if(row.Length!=23 || row[18]!=style.Collision || row[19]!="1" ||
                    row[20]!="0" || row[21]!="0" || row[22]!=style.Platform)
                    throw new InvalidOperationException(style.Name+" platform wire contract mismatch");
            }
            foreach(var item in new[] {
                (Name:"ProbeCustomCollisionBox",Mode:"0",Platform:"0",Width:"18",Height:"12",X:"2.5",Y:"-3"),
                (Name:"ProbeCollisionBoxWithPlatforms",Mode:"2",Platform:"1",Width:"12",Height:"6",X:"0",Y:"0")
            }) {
                string[] row=rows.Single(x=>
                    x.StartsWith("PROJECTILE\t"+item.Name+"\t",StringComparison.Ordinal)).Split('\t');
                if(row.Length!=27 || row[18]!=item.Mode || row[22]!=item.Platform ||
                   row[23]!=item.Width || row[24]!=item.Height ||
                   row[25]!=item.X || row[26]!=item.Y)
                    throw new InvalidOperationException(item.Name+" hitbox wire contract mismatch");
            }
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
