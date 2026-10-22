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
        int height = Get<int>("CoordinateHeight");
        int adjacent = Get<int>("AdjacentTile");
        string dust = Get<string>("DustName");
        string map = Get<string>("MapName");
        int r = Get<int>("MapR"), g = Get<int>("MapG"), b = Get<int>("MapB");
        int fail = (int)(type.GetMethod("NumDust")?.Invoke(tile, new object[] {true})
            ?? throw new InvalidOperationException(type.FullName + ": NumDust missing"));
        int normal = (int)(type.GetMethod("NumDust")?.Invoke(tile, new object[] {false})
            ?? throw new InvalidOperationException(type.FullName + ": NumDust missing"));
        if ((flags & ~8191) != 0 || shape < 0 || shape > 1 ||
            height < 1 || height > 32 || adjacent < -1 || adjacent >= 754 ||
            dust.Length > 72 || !Safe(dust) || map.Length > 80 || !Safe(map) ||
            (uint)r > 255 || (uint)g > 255 || (uint)b > 255 ||
            fail < 0 || fail > 20 || normal < 0 || normal > 20 ||
            (shape == 0 && height != 18) || (shape == 1 && height != 18))
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
            normal.ToString(CultureInfo.InvariantCulture));
    }

    private static bool Safe(string value) {
        foreach (char c in value)
            if (!(char.IsAsciiLetterOrDigit(c) || c == '_' || c == '-' || c == ' '))
                return false;
        return true;
    }
}
