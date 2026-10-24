using System;
using System.Reflection;
using BunnyLoader.Source;

#if INVALID_NAME
namespace InvalidCase
{
    public sealed class São : BunnyWorldSystem { }
}
#else
namespace FirstCase
{
    public sealed class Progress : BunnyWorldSystem { }
}

namespace SecondCase
{
    public sealed class Progress : BunnyWorldSystem { }
}
#endif

internal static class Program
{
    public static int Main()
    {
        string path = Assembly.GetExecutingAssembly().Location;
        int slot = BunnyEntitySourceDispatcher.Register("world-identity-test", path);
#if INVALID_NAME
        const string expected = "Invalid world system class São";
#else
        const string expected = "Duplicate world system class Progress";
#endif
        if (slot >= 0 || !BunnyEntitySourceDispatcher.LastRegistrationError.Contains(
            expected, StringComparison.Ordinal))
            throw new InvalidOperationException(
                "World identity was not rejected: " +
                BunnyEntitySourceDispatcher.LastRegistrationError);
        Console.WriteLine("World identity collision and wire validation: PASS " + expected);
        return 0;
    }
}
