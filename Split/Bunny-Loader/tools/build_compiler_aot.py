"""Build normal (mixed JIT/AOT) compiler modules. Mod sources are never AOT compiled."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

ASSEMBLIES = (
    "System.Private.CoreLib.dll", "System.Collections.Immutable.dll",
    "System.Reflection.Metadata.dll", "Microsoft.CodeAnalysis.dll",
    "Microsoft.CodeAnalysis.CSharp.dll", "BunnySourceCompiler.dll",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(framework, cross, clang, mono, output, work):
    for path in (cross, clang, mono):
        if not path.is_file():
            raise ValueError("AotToolMissing: " + str(path))
    work.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MONO_PATH=str(framework.resolve()), MONO_ENV_OPTIONS="")
    rows, objects = [], []
    for name in ASSEMBLIES:
        assembly = framework / name
        if not assembly.is_file():
            raise ValueError("AotAssemblyMissing: " + name)
        source, obj = work / (name + ".s"), work / (name + ".o")
        started = time.monotonic()
        # Normal mode is deliberate: full/hybrid modes would restrict editable JIT mods.
        subprocess.run([str(cross.resolve()), "--nollvm",
                        "--aot=static,asmonly,nodebug,mtriple=aarch64-linux-android,outfile=" + str(source.resolve()),
                        str(assembly.resolve())], env=env, check=True, timeout=600)
        subprocess.run([str(clang), "-c", "-fPIC", str(source), "-o", str(obj)], check=True)
        objects.append(str(obj))
        rows.append({"assembly": name, "sha256": digest(assembly),
                     "buildMs": round((time.monotonic() - started) * 1000),
                     "objectBytes": obj.stat().st_size})
    code = ["#include <stddef.h>",
            "struct Module { const char *assembly, *sha256; void *info; };",
            "#define API __attribute__((visibility(\"default\")))",
            "API int bunny_compiler_aot_abi(void) { return 1; }",
            'API const char *bunny_compiler_aot_runtime_hash(void) { return "' + digest(mono) + '"; }',
            "API int bunny_compiler_aot_count(void) { return " + str(len(rows)) + "; }"]
    symbols = ["mono_aot_module_" + re.sub(r"[^A-Za-z0-9_]", "_", row["assembly"][:-4]) + "_info"
               for row in rows]
    for symbol in symbols:
        code.append("extern void *" + symbol + ";")
    code.append("static struct Module modules[] = {")
    code.extend('{"' + row["assembly"] + '","' + row["sha256"] + '",NULL},' for row in rows)
    code.extend(["};", "API const struct Module *bunny_compiler_aot_module(int i) { switch(i) {"])
    code.extend("case " + str(i) + ": modules[i].info = " + symbol + "; return &modules[i];"
                for i, symbol in enumerate(symbols))
    code.extend(["default: return NULL; } }"])
    shim = work / "compiler-aot-table.c"
    shim.write_text("\n".join(code) + "\n")
    subprocess.run([str(clang), "-shared", "-fPIC", "-fvisibility=hidden",
                    "-Wl,--no-undefined", "-Wl,-z,max-page-size=16384",
                    "-Wl,-soname,libbunnycompileraot.so", *objects,
                    str(shim), str(mono), "-o", str(output)], check=True)
    report = {"runtime": "8.0.23", "mode": "Normal", "llvm": False,
              "modAssembliesCompiled": 0, "modules": rows,
              "bundleBytes": output.stat().st_size, "bundleSha256": digest(output),
              "androidValidated": False, "fiveSecondTargetValidated": False}
    (work / "compiler-aot-build.json").write_text(json.dumps(report, indent=2) + "\n")
    print("COMPILER_NORMAL_AOT_BUILT modules=" + str(len(rows)) + " bytes=" + str(output.stat().st_size))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("framework", "cross", "clang", "mono", "output", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    build(**vars(parser.parse_args()))
