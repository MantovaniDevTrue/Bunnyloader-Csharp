from pathlib import Path
import ast
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "app/src/main/cpp/experiments/csharp/UniversalSystemStages.cpp").read_text()


def macro(name, next_name):
    begin = "#define BL_CSHARP_SYSTEM_" + name
    end = "#define BL_CSHARP_SYSTEM_" + next_name
    if begin not in source or end not in source:
        raise AssertionError("Macro ARM64 ausente: " + name)
    text = source.split(begin, 1)[1].split(end, 1)[0]
    strings = re.findall(r'^\s*("(?:\\.|[^"\\])*")', text, flags=re.MULTILINE)
    assert strings, name + " sem instruções"
    return "".join(ast.literal_eval(value) for value in strings)


save = macro("SAVE_FRAME", "RESTORE_FRAME")
restore = macro("RESTORE_FRAME", "BRIDGE")
assert "sub sp, sp, #768" in save
assert "add sp, sp, #768" in restore
assert "mrs x9, nzcv" in save
assert "msr nzcv, x9" in restore
assert "stp q30, q31" in save
assert "ldp q30, q31" in restore

assembly = (
    ".text\n.p2align 2\n.global bunny_csharp_stage_smoke\n"
    "bunny_csharp_stage_smoke:\n"
    + save
    + "mov x1, sp\nmov w0, #6\n"
    + restore
    + "adrp x16, g_blCSharpSystemOriginal6\n"
    + "ldr x16, [x16, :lo12:g_blCSharpSystemOriginal6]\n"
    + "br x16\n"
)
with tempfile.TemporaryDirectory() as tmp:
    asm = Path(tmp) / "stage.S"
    obj = Path(tmp) / "stage.o"
    asm.write_text(assembly)
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-c",
                    str(asm), "-o", str(obj)], check=True)
    assert obj.stat().st_size > 0
    info = subprocess.check_output(["file", str(obj)], text=True)
    assert "ARM aarch64" in info or "AArch64" in info, info

print("Trampolim ARM64: registradores, flags e salto original montados sem APK.")
