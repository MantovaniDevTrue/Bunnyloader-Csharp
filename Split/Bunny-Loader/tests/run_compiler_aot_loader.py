"""Execute the production dlopen/hash/ABI gate; fixture modules are not a Mono execution test."""
import hashlib
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="bunny-aot-test-") as temp:
    work = Path(temp)
    driver = work / "loader"
    subprocess.run(["g++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fno-exceptions", "-fno-rtti", "-I", str(root / "app/src/main/cpp"),
                    str(root / "tests/compiler_aot_loader.cpp"), "-ldl", "-o", str(driver)], check=True)
    mono = work / "libmonosgen-2.0.so"
    mono.write_bytes(b"runtime fixture")
    (work / "Compiler.dll").write_bytes(b"compiler fixture")
    (work / "Metadata.dll").write_bytes(b"metadata fixture")
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    runtime_sha, compiler_sha, metadata_sha = sha(mono), sha(work / "Compiler.dll"), sha(work / "Metadata.dll")
    def run(expected, source=None):
        bundle = work / "libfixture.so"
        if source is not None:
            file = work / "fixture.c"
            file.write_text(source)
            subprocess.run(["cc", "-shared", "-fPIC", str(file), "-o", str(bundle)], check=True)
        subprocess.run([str(driver), expected, str(bundle), str(work), str(mono)], check=True)
    code = f'''#include <stdint.h>
struct Module {{ const char *assembly, *sha256; void *info; }};
static uint32_t info1 = 185, info2 = 185;
int bunny_compiler_aot_abi(void) {{ return 1; }}
const char *bunny_compiler_aot_runtime_hash(void) {{ return "{runtime_sha}"; }}
int bunny_compiler_aot_count(void) {{ return 2; }}
const struct Module *bunny_compiler_aot_module(int i) {{
 static struct Module m[] = {{ {{"Compiler.dll","{compiler_sha}",&info1}}, {{"Metadata.dll","{metadata_sha}",&info2}} }};
 return &m[i]; }}
'''
    run("BundleUnavailable")
    run("BundleAbiMismatch", "int fixture(void) { return 0; }")
    run("RuntimeHashMismatch", code.replace(runtime_sha, "0" * 64))
    run("AotFormatMismatch:Metadata.dll", code.replace("info2 = 185", "info2 = 184"))
    run("AssemblyHashMismatch:Metadata.dll", code.replace(metadata_sha, "0" * 64))
    run("AssemblyNameInvalid", code.replace("Metadata.dll", "../Metadata.dll"))
    run("ModuleCountInvalid", code.replace("return 2;", "return 17;"))
    run("valid", code)
    # A same-sized changed dependency must reject all modules, including the first valid one.
    (work / "Metadata.dll").write_bytes(b"metadata corrupt")
    run("AssemblyHashMismatch:Metadata.dll")
