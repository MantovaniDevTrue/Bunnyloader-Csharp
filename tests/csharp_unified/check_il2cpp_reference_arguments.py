"""IL2CPP: valores de tipo referencia vao como o proprio objeto.

il2cpp_field_static_set_value -> Field::SetValueRaw(deref_pointer=false): para
string/array/classe grava `value` direto no campo. runtime_invoke: o invoker
gerado faz (Il2CppObject*)args[i] para parametro referencia (so `out`/`ref`
recebem endereco). Passar &variavel_local grava um endereco de pilha no jogo:
foi o crash SEGV_ACCERR em Main.AnglerQuestSwap (libil2cpp+0x1019714, leitura de
anglerQuestItemNetIDs) e o CoordinateFullHeight lixo do ExampleSapling.
"""
from pathlib import Path
import re

root = Path("app/src/main/cpp")
REF = r"(?:Il2CppArray|Il2CppObject|Il2CppString)\s*\*\s*(?:const\s+)?{v}\b"
errors = []
for path in sorted(root.rglob("*")):
    if path.suffix not in (".cpp", ".h") or "third_party" in path.parts:
        continue
    text = path.read_text(errors="ignore")
    for m in re.finditer(r"field_static_set_value\(\s*([^,]+?)\s*,\s*&(\w+)\s*\)", text):
        start = text.rfind("\n}", 0, m.start())
        body = text[start if start >= 0 else 0:m.start()]
        if re.search(REF.format(v=re.escape(m.group(2))), body):
            line = text.count("\n", 0, m.start()) + 1
            errors.append(f"{path}:{line}: field_static_set_value com &{m.group(2)} (tipo referencia)")

def source(name):
    return (root / name).read_text()

angler = source("experiments/csharp/NativeAnglerQuestGameplay.cpp")
assert "field_static_set_value(g_questItems, nextIds)" in angler
assert "field_static_set_value(plan.field, plan.array)" in angler
sapling = source("experiments/csharp/NativeTileSaplingShape.h")
assert "slot.reference?*static_cast<void* const*>(value)" in sapling
assert 'std::strstr(type,"[]")!=nullptr' in sapling
batch = source("experiments/csharp/NativeTileSpriteBatchLifecycle.cpp")
assert "{&sort,blend,sampler,depth,raster,effect,nullable.data(),&deferred}" in batch
assert "void* args[1] = {key};" in source("experiments/csharp/NativeTouchGroups.cpp")
assert "textureArgument,const_cast<uint32_t*>(&rgba)" in source("experiments/csharp/NativeTooltipPanelBacking.h")
assert "void* args[]={sound,position,&pitch,&volume};" in source("experiments/csharp/NativeTileGameplay.cpp")
if errors:
    raise SystemExit("IL2CPP_REFERENCE_ARGUMENTS=FAIL\n" + "\n".join(errors))
print("IL2CPP_REFERENCE_ARGUMENTS=PASS")
