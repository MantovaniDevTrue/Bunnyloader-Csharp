from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
cpp = (root / "app/src/main/cpp/experiments/csharp/SourceLimits.h").read_text()
roslyn = (root / "experiments/csharp/BunnySourceCompiler/Compiler.cs").read_text()
native = (root / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
probe = (root / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text()
loader = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
parser = (root / "app/src/main/cpp/experiments/csharp/SourceParser.cpp").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
limits = {
    "kSourceMaxFiles": 4096,
    "kSourceMaxFileBytes": 1024*1024,
    "kSourceMaxTotalBytes": 32*1024*1024,
    "kSourceMaxTypes": 8192,
    "kSourceMaxCategory": 4096,
    "kSourceDefaultsBytes": 8*1024*1024,
    "kSourceMotionBytes": 4*1024*1024,
    "kSourceMaxAssemblyBytes": 32*1024*1024,
}
for name, expected in limits.items():
    match = re.search(r"\b"+name+r"\s*=\s*([\d* ]+);", cpp)
    assert match, name
    actual = eval(match.group(1), {"__builtins__": {}}, {})
    assert actual == expected, f"{name}={actual} expected={expected}"
for key, value in {
    "MaxFiles": 4096,
    "MaxFileBytes": 1024*1024,
    "MaxTotalBytes": 32*1024*1024,
}.items():
    expr = re.search(r"private const int "+key+r"\s*=\s*([\d *]+);", roslyn)
    assert expr and eval(expr.group(1), {"__builtins__": {}}, {}) == value, key
assert "source.size() > kSourceMaxFileBytes" in parser
assert "kSourceMaxTotalBytes" in native
assert "metadata.st_size > static_cast<off_t>(kSourceMaxFileBytes)" in loader
assert "kSourceMaxAssemblyBytes" in loader
assert "Length > 32 * 1024 * 1024" in probe
assert "st.st_size <= 32 * 1024 * 1024" in runtime
assert "tileTypes.Length > 4096" in probe
assert "potionTypes.Length > 4096" in probe
assert "trees.Count & 63" in roslyn
assert "sourcesMs=" in roslyn and "parsingMs=" in roslyn and "emitMs=" in roslyn
print("BUNNY_CSHARP_SCALE_LIMITS_AND_PROFILE=PASS")
