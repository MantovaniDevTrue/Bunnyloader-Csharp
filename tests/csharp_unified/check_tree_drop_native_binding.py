from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "app/src/main/cpp/content/tiles/ModTrees.cpp").read_text()
start = source.index("bool readNativeAcornType() {")
end = source.index("void hkGetTreeDrops(",start)
acorn = source[start:end]
assert 'System.Int16' in acorn
assert 'System.Int32' in acorn
assert 'dlopen("libil2cpp.so",RTLD_NOW|RTLD_NOLOAD)' in acorn
assert 'tree_acorn::resolveId(nativeValue)' in acorn
assert 'nativeValue<0?"TerrariaVanillaID":"IL2CPP"' in acorn
assert "dlsym(RTLD_DEFAULT" not in acorn

start = source.index("bool prepareModTreeDrops(")
end = source.index("bool configureModTreeDrops(",start)
binding = source[start:end]
assert 'if(!readNativeAcornType())' in binding
assert 'if(!bindTreeRandom())' in binding
assert 'KillTile_GetTreeDrops' in binding
assert 'hook::install(method,hkGetTreeDrops,&g_originalTreeDrops)' in binding
assert 'Reason=MainRandNextDoubleUnavailable' in binding

start = source.index("void hkGetTreeDrops(")
end = source.index("bool installHooks()",start)
drops = source[start:end]
assert 'g_originalTreeDrops(x,y,tile,isWood,bonusWood,primary,secondary,method)' in drops
assert 'tree_drop::apply(' in drops
assert 'rollTopAcorn(' in drops
print("CSHARP_TREE_DROPS_NATIVE_BINDING_PASS")
