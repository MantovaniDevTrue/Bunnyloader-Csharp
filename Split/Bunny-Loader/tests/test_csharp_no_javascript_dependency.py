"""Architectural guard: C# loading/gameplay cannot call a scripting engine."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
native=root/'app/src/main/cpp'
for source in (native/'experiments/csharp').glob('*.cpp'):
    code=source.read_text()
    for forbidden in ('JS_Call(', 'JS_Eval(', 'JS_GetGlobalObject(', 'JsLock ', 'script::engine()', '__blSyncCSharp'):
        assert forbidden not in code,(source.name,forbidden)
cmake=(native/'CMakeLists.txt').read_text().split('set(BL_MOD_CLASSES_PARTS',1)[1].split(')',1)[0]
for name in ('CSharpItemBehaviorLoader','CSharpArmorPolicy','CSharpWingPolicy'):
    assert name not in cmake,name
    assert not (native/'script/js/mod/Loaders'/f'{name}.js').exists()
for name in ('EquipLoader','RecipeLoader','PlayerDrawHooks'):
    code=(native/'script/js/mod/Loaders'/f'{name}.js').read_text()
    assert 'SyncCSharp' not in code and 'bl.csharp' not in code,name
assert 'script::engine().init()' not in (native/'boot/Probe.cpp').read_text()
loader=(native/'mods/ModLoader.cpp').read_text()
source=loader.index('bl::csharp::loadCSharpSourceMod(')
assert source<loader.index('if (!script::engine().ready() && !script::engine().init())',source)
test=(root/'tests/run_native_item_policies.py').read_text()
assert '-DBL_HAVE_QUICKJS=0' in test
gate=(native/'experiments/csharp/AutoSourceRuntime.cpp').read_text()
assert 'const size_t pendingNativePolicies=pendingNativeItemPolicies();' in gate
assert 'pendingCore>0&&static_cast<size_t>(pendingCore)<=pendingNativePolicies' in gate
assert 'elapsed>=6&&(pendingCore==0||deferredNativePolicies)' in gate
assert 'g_initialCSharpRegistrationFinished.load(std::memory_order_acquire)' in gate
assert 'g_preWorldBindingsArmed.load(std::memory_order_acquire)' in gate
assert 'bl::runtime::modItemsSettled()' in gate
assert 'Stage=PendingNativePolicy' in gate
assert 'const int pendingCore=std::max(0,content.pendingBehaviors-pendingArmor-pendingEquipment);' in gate
print('BUNNY_CSHARP_PREWORLD_NATIVE_POLICY_DEFER_GUARDS=PASS')
print('BUNNY_CSHARP_NO_JAVASCRIPT_LOADING_GAMEPLAY_OR_EQUIPMENT=PASS')
