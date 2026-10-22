#!/usr/bin/env python3
"""Production pinned Mono queue and parser tests, using a test-only Mono ABI double. No APK."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cpp = root / "app/src/main/cpp"
tests = Path(__file__).resolve().parent / "native"
flags = ["g++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-pthread", "-I" + str(cpp)]
with tempfile.TemporaryDirectory(prefix="bunny-content-native-") as temp:
    out = Path(temp)
    subprocess.run(flags + [str(cpp / "experiments/csharp/SourceParser.cpp"), str(tests / "parser_test.cpp"), "-o", str(out / "parser")], check=True)
    subprocess.run([str(out / "parser")], check=True, timeout=15)
    subprocess.run(flags + ["-shared", "-fPIC", str(tests / "mono_mock.cpp"), "-o", str(out / "libmonosgen-2.0.so")], check=True)
    subprocess.run(flags + [str(cpp / "experiments/csharp/ManagedRuntime.cpp"), str(tests / "worker_test.cpp"), "-ldl", "-o", str(out / "worker")], check=True)
    subprocess.run(flags + ["-I" + str(root / "tests/basic_items/include"), str(cpp / "experiments/csharp/SourceParser.cpp"), str(tests / "source_loader_test.cpp"), "-o", str(out / "source-loader")], check=True)
    subprocess.run([str(out / "source-loader"), str(root / "samples/ContentLoaderCSharp/Playable")], check=True, timeout=15)
    startup_flags = flags + ["-ffunction-sections", "-fdata-sections", "-I" + str(root / "tests/startup/include"), "-I" + str(root / "tests/basic_items/include")]
    subprocess.run(startup_flags + [str(root / "tests/auto_source_startup.cpp"), "-Wl,--gc-sections", "-o", str(out / "startup")], check=True)
    subprocess.run([str(out / "startup")], check=True, timeout=15)
    subprocess.run([str(out / "startup"), "field-only"], check=True, timeout=15)
    probe = out / "probe.dll"
    probe.write_bytes(b"test-only ABI fixture")
    (out / "BunnySourceSdk.dll").write_bytes(b"test-only SDK ABI fixture")
    env = dict(os.environ, LD_LIBRARY_PATH=str(out))
    for mode in ["no-worker", "worker"]:
        subprocess.run([str(out / "worker"), str(probe), mode], env=env, check=True, timeout=20)
