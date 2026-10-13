using System;
using System.IO;

namespace BunnyLoader.SourceCompiler;

public static class Preflight
{
    public static int Ping(string diagnosticPath)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(diagnosticPath)!);
        File.WriteAllText(diagnosticPath, "ROSLYN_PREFLIGHT=42");
        return 42;
    }
}

