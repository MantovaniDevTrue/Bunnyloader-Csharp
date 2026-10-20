using System;
using System.Globalization;
using System.Reflection;
using System.Text;

namespace BunnyLoader.SourceCompiler;

internal static class TileWriter {
    internal static string Write(object tile) {
        Type type = tile.GetType();
        T Get<T>(string name) => (T)(type.GetProperty(name)?.GetValue(tile)
            ?? throw new InvalidOperationException(type.FullName + ": missing tile property " + name));
        int flags = Convert.ToInt32(Get<object>("Flags"), CultureInfo.InvariantCulture);
        int shape = Convert.ToInt32(Get<object>("Shape"), CultureInfo.InvariantCulture);
        int toggle = Convert.ToInt32(Get<object>("FrameToggle"), CultureInfo.InvariantCulture);
        int height = Get<int>("CoordinateHeight");
        int animationHeight = Get<int>("AnimationFrameHeight");
        int animationFrames = Get<int>("AnimationFrames");
        int animationTicks = Get<int>("AnimationFrameTicks");
        int adjacent = Get<int>("AdjacentTile");
        int[] aliases = Get<int[]>("AdjacentTiles");
        bool allowBreaking = Get<bool>("AllowBreaking");
        bool allowDrops = Get<bool>("AllowDrops");
        bool cacheDrawData = Get<bool>("CacheDrawData");
        string itemDrops = Get<string>("ItemDropDefinitions");
        int minPick = Get<int>("MinPick");
        float mineResist = Get<float>("MineResist");
        int callbackMask = Get<int>("TileCallbackMask");
        string dust = Get<string>("DustName");
        string map = Get<string>("MapName");
        int r = Get<int>("MapR"), g = Get<int>("MapG"), b = Get<int>("MapB");
        if (aliases.Length > 16) throw new InvalidOperationException(type.FullName + ": maximum 16 adjacent tile aliases");
        int[] sortedAliases = (int[])aliases.Clone();
        Array.Sort(sortedAliases);
        for (int i = 0; i < sortedAliases.Length; i++)
            if (sortedAliases[i] < 0 || sortedAliases[i] >= 754 ||
                sortedAliases[i] == adjacent ||
                (i > 0 && sortedAliases[i] == sortedAliases[i - 1]))
                throw new InvalidOperationException(type.FullName + ": invalid or duplicate adjacent tile");
        string adjacentList = sortedAliases.Length == 0 ? "-" :
            string.Join(",", Array.ConvertAll(sortedAliases,
                value => value.ToString(CultureInfo.InvariantCulture)));
        int fail = (int)(type.GetMethod("NumDust",new[] {typeof(bool)})?.Invoke(tile, new object[] {true})
            ?? throw new InvalidOperationException(type.FullName + ": NumDust missing"));
        int normal = (int)(type.GetMethod("NumDust",new[] {typeof(bool)})?.Invoke(tile, new object[] {false})
            ?? throw new InvalidOperationException(type.FullName + ": NumDust missing"));
        if ((flags & ~131071) != 0)
            throw new InvalidOperationException("[CSharp][ERROR][TileDefaults] Class=" + type.FullName +
                " API=BunnyTile.Flags Field=Flags Stage=Serialize Expected=Int32:KnownMask0x1FFFF Actual=" +
                flags.ToString(CultureInfo.InvariantCulture) + " Result=Rejected Reason=UnknownFlagBits");
        if (shape < 0 || shape > 3 || toggle < 0 || toggle > 2 ||
            (toggle == 1 && shape != 2) || (toggle == 2 && shape != 3) ||
            height < 1 || height > 32 || adjacent < -1 || adjacent >= 754 ||
            dust.Length > 72 || !Safe(dust) || map.Length > 80 || !Safe(map) ||
            (uint)r > 255 || (uint)g > 255 || (uint)b > 255 ||
            fail < 0 || fail > 20 || normal < 0 || normal > 20 ||
            animationHeight < 0 || animationHeight > 4096 ||
            animationFrames < 1 || animationFrames > 64 ||
            animationTicks < 0 || animationTicks > 600 ||
            (animationFrames > 1 && (animationHeight == 0 || animationTicks == 0)) ||
            (animationFrames == 1 && animationTicks != 0) ||
            height != 18 || minPick < 0 || minPick > 1000 ||
            !float.IsFinite(mineResist) || mineResist < 0f || mineResist > 128f)
            throw new InvalidOperationException(type.FullName + ": native tile defaults out of bounds");
        return string.Join("\t", "TILE", type.Name,
            flags.ToString(CultureInfo.InvariantCulture),
            shape.ToString(CultureInfo.InvariantCulture),
            height.ToString(CultureInfo.InvariantCulture),
            adjacent.ToString(CultureInfo.InvariantCulture),
            dust.Length == 0 ? "-" : dust,
            r.ToString(CultureInfo.InvariantCulture), g.ToString(CultureInfo.InvariantCulture),
            b.ToString(CultureInfo.InvariantCulture),
            map.Length == 0 ? "-" : Convert.ToHexString(Encoding.UTF8.GetBytes(map)),
            fail.ToString(CultureInfo.InvariantCulture),
            normal.ToString(CultureInfo.InvariantCulture),
            animationHeight.ToString(CultureInfo.InvariantCulture),
            animationFrames.ToString(CultureInfo.InvariantCulture),
            animationTicks.ToString(CultureInfo.InvariantCulture),
            adjacentList,
            allowBreaking ? "1" : "0",
            allowDrops ? "1" : "0",
            callbackMask.ToString(CultureInfo.InvariantCulture),
            cacheDrawData ? "1" : "0",
            toggle.ToString(CultureInfo.InvariantCulture),
            itemDrops,
            minPick.ToString(CultureInfo.InvariantCulture),
            mineResist.ToString("R", CultureInfo.InvariantCulture));
    }

    private static bool Safe(string value) {
        foreach (char c in value)
            if (!(char.IsAsciiLetterOrDigit(c) || c == '_' || c == '-' || c == ' '))
                return false;
        return true;
    }
}
