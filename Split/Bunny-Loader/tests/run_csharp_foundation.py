#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cpp = root / "app/src/main/cpp"
flags = ["g++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-pthread", "-I" + str(cpp)]
with tempfile.TemporaryDirectory(prefix="bunny-foundation-") as temporary:
    out = Path(temporary)
    for test, sources in {
        "managed_content_declarations": [cpp / "experiments/csharp/SourceParser.cpp"],
        "managed_continuous_queue": [],
    }.items():
        executable = out / test
        subprocess.run(flags + [str(root / "tests" / (test + ".cpp"))] +
                       [str(path) for path in sources] + ["-o", str(executable)], check=True)
        arguments = [str(root / "samples/DeclaredContentCSharp/Playable"), str(root / "samples/NativeAccessoryCSharp/Playable")] if test == "managed_content_declarations" else []
        subprocess.run([str(executable)] + arguments, check=True, timeout=30)
    executable = out / "auto-source-cache"
    subprocess.run(flags + ["-ffunction-sections", "-fdata-sections",
                           "-I" + str(root / "tests/startup/include"),
                           "-I" + str(root / "tests/basic_items/include"),
                           str(root / "tests/auto_source_cache.cpp"),
                           "-Wl,--gc-sections", "-o", str(executable)], check=True)
    subprocess.run([str(executable), str(out / "cache-fixture")], check=True, timeout=30)
    subprocess.run(flags + ["-shared", "-fPIC", str(root / "tests/managed_fault_mock.cpp"),
                           "-o", str(out / "libmonosgen-2.0.so")], check=True)
    executable = out / "continuous-runtime"
    subprocess.run(flags + [str(cpp / "experiments/csharp/ManagedRuntime.cpp"),
                           str(root / "tests/managed_continuous_runtime.cpp"),
                           "-ldl", "-o", str(executable)], check=True)
    probe = out / "probe.dll"
    probe.write_bytes(b"mock")
    subprocess.run([str(executable), str(probe)],
                   env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=45)
    subprocess.run(flags + ["-shared", "-fPIC", str(root / "tests/managed_hurt_deadline_mock.cpp"),
                           "-o", str(out / "libmonosgen-2.0.so")], check=True)
    (out / "BunnySourceSdk.dll").write_bytes(b"mock")
    executable = out / "hurt-deadline"
    subprocess.run(flags + [str(cpp / "experiments/csharp/ManagedRuntime.cpp"),
                           str(root / "tests/managed_hurt_deadline.cpp"),
                           "-ldl", "-o", str(executable)], check=True)
    subprocess.run([str(executable), str(probe)],
                   env=dict(os.environ, LD_LIBRARY_PATH=str(out)), check=True, timeout=20)
