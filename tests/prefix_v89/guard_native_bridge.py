from pathlib import Path

root = Path(__file__).resolve().parents[2]
src = root / "app/src/main/cpp/experiments/csharp"
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text()
parser = (src / "SourceParser.cpp").read_text()
loader = (src / "SourceLoader.cpp").read_text()
bridge = (src / "ManagedPrefixBridge.cpp").read_text()
worker = (root / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text()

assert "public abstract class BunnyPrefix" in sdk
assert "SetStats(ref PrefixStats stats)" in sdk
kinds = (src / "SourceContentKinds.generated.h").read_text()
assert "isRecognizedSourceBase(base)" in parser and '"BunnyPrefix"' in kinds
assert "registerModPrefix(mod.id, def.name)" in loader
assert 'fields[0] == "PREFIX" && (fields.size() == 16 || fields.size() == 17)' in loader
assert 'record.itemWeights.emplace(itemType, weight)' in loader
assert "applyManagedPrefixDescriptors(modId, prefixes)" in loader
assert "modPrefixByKey(modId + \"/\" + entry.name)" in bridge
assert 'lines.Add(string.Join("\\t", fields))' in worker
assert "prefixTypes.Select(CreateContent)" in worker
assert "applyManagedPrefixDescriptors" in bridge
assert "install(" not in bridge
print("BUNNY_CSHARP_PREFIX_NATIVE_STAGING=PASS runtime_hooks=pending")
