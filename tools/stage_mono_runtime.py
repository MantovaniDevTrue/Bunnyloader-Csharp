import hashlib
import sys
import zipfile
from pathlib import Path

archive_path, native_dir, assembly_dir = map(Path, sys.argv[1:4])
native_dir.mkdir(parents=True, exist_ok=True)
assembly_dir.mkdir(parents=True, exist_ok=True)

native_count = 0
managed_count = 0

with zipfile.ZipFile(archive_path) as archive:
    filenames = archive.namelist()
    native_files = [
        name for name in filenames
        if name.startswith("runtimes/android-arm64/native/")
        and name.endswith(".so")
        and name.count("/") == 3
    ]
    assemblies = [
        name for name in filenames
        if name.startswith("runtimes/android-arm64/lib/")
        and name.endswith(".dll")
        and name.count("/") == 4
    ]
    corelibs = [
        name for name in filenames
        if name.startswith("runtimes/android-arm64/native/")
        and name.endswith("/System.Private.CoreLib.dll")
    ]
    if len(corelibs) != 1:
        raise SystemExit("Expected exactly one Android ARM64 System.Private.CoreLib.dll")
    if not any(name.endswith("/libmonosgen-2.0.so") for name in native_files):
        raise SystemExit("Missing libmonosgen-2.0.so in official Mono runtime pack")
    if not any(name.endswith("/libSystem.Native.so") for name in native_files):
        raise SystemExit("Missing libSystem.Native.so in official Mono runtime pack")
    if not any(name.endswith("/System.Runtime.dll") for name in assemblies):
        raise SystemExit("Missing System.Runtime.dll in official Mono runtime pack")
    for name in sorted(native_files):
        basename = Path(name).name
        if basename == "libc++_shared.so":
            continue
        destination = native_dir / basename
        destination.write_bytes(archive.read(name))
        native_count += 1
        print(f"MONO_NATIVE {basename} {destination.stat().st_size}")
    for name in sorted(assemblies):
        basename = Path(name).name
        destination = assembly_dir / basename
        destination.write_bytes(archive.read(name))
        managed_count += 1
    (assembly_dir / "System.Private.CoreLib.dll").write_bytes(archive.read(corelibs[0]))

if native_count < 2 or managed_count < 1:
    raise SystemExit("Incomplete Android ARM64 Mono runtime payload")
print(f"MONO_NATIVE_COUNT={native_count}")
print(f"MONO_FRAMEWORK_ASSEMBLIES={managed_count + 1}")
print(f"MONO_NATIVE_READY=YES")
print(f"MONO_FRAMEWORK_READY=YES")
