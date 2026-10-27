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

import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("app/src/main/cpp")
PTR = r"(?:Il2CppArray|Il2CppObject|Il2CppString|Il2CppReflectionType|Il2CppDelegate|Il2CppException)"
OBJ_RHS = (r"(?:readObject|array_new|string_new|object_new|runtime_invoke|gchandle_get_target|value_box|"
           r"(?:reinterpret|static)_cast<\s*" + PTR + r"\s*\*\s*>|field<\s*" + PTR + r"\s*\*\s*>)")
# Parametros `out`/`ref` recebem o endereco de proposito (IL2CPP byref).
BYREF_ALLOWED = {
    ("menu/Powers.cpp", "id"),     # BestiaryCreditIdsLookup.TryGetValue(netId, out string id)
    ("menu/Powers.cpp", "power"),  # Dictionary.TryGetValue(key, out ICreativePower power)
    ("experiments/csharp/NativeGlobalItems.cpp", "sample"),  # ContentSamples.ItemsByType.TryGetValue(int, out Item)
}

def function_body(text, pos):
    start = text.rfind("\n}", 0, pos)
    return text[start if start >= 0 else 0:pos]

def is_object(body, var):
    v = re.escape(var)
    return bool(re.search(PTR + r"\s*\*\s*(?:const\s+)?" + v + r"\b", body) or
                re.search(r"auto\s*\*\s*(?:const\s+)?" + v + r"\s*=\s*[^;]*" + OBJ_RHS, body) or
                re.search(r",\s*\*\s*" + v + r"\s*=\s*[^;,]*" + OBJ_RHS, body))

errors = []
for path in sorted(root.rglob("*")):
    if path.suffix not in (".cpp", ".h") or "third_party" in path.parts:
        continue
    rel = path.relative_to(root).as_posix()
    text = path.read_text(errors="ignore")
    def report(pos, what):
        errors.append(f"{rel}:{text.count(chr(10), 0, pos) + 1}: {what}")
    # il2cpp_field_static_set_value: referencia vai como objeto.
    for m in re.finditer(r"field_static_set_value\(\s*[^,]+?\s*,\s*&\s*(\w+)\s*\)", text):
        if is_object(function_body(text, m.start()), m.group(1)):
            report(m.start(), f"field_static_set_value com &{m.group(1)} (tipo referencia)")
    # runtime_invoke: void* args[] = {..., &obj, ...}
    for m in re.finditer(r"void\s*\*\s*(\w+)\s*\[\s*\w*\s*\]\s*=\s*\{([^}]*)\}", text):
        body = function_body(text, m.start())
        for var in re.findall(r"&\s*([A-Za-z_]\w*)\b(?!\s*(?:[.\[(]|->))", m.group(2)):
            if (rel, var) not in BYREF_ALLOWED and is_object(body, var):
                report(m.start(), f"argumento &{var} de tipo referencia em {m.group(1)}[]")
    # args[i] = &obj;
    for m in re.finditer(r"\b(\w+)\s*\[\s*\d+\s*\]\s*=\s*&\s*([A-Za-z_]\w*)\s*;", text):
        body = function_body(text, m.start())
        if (re.search(r"void\s*\*\s*" + re.escape(m.group(1)) + r"\s*\[", body) and
                (rel, m.group(2)) not in BYREF_ALLOWED and is_object(body, m.group(2))):
            report(m.start(), f"{m.group(1)}[i] = &{m.group(2)} (tipo referencia)")

if len(sys.argv) > 1:   # modo varredura de uma arvore qualquer (sem os asserts do repo)
    print("\n".join(errors) or "nenhum")
    raise SystemExit(1 if errors else 0)

def source(name):
    return (Path("app/src/main/cpp") / name).read_text()

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
