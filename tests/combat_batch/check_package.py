"""Validate the example package and the shared source-loading limits before APK build."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
example = root / 'samples/ExampleModCSharp'
playable = example / 'Playable'
manifest = json.loads((playable / 'manifest.json').read_text())
compat = json.loads((example / 'compatibility.json').read_text())
assert manifest['version'] == compat['exampleModCSharpVersion']
sources = sorted((playable / 'content').rglob('*.cs'))
assert sources and len(sources) <= 4096, f'Source capacity exceeded: {len(sources)}'
assert sum(p.stat().st_size for p in sources) <= 32 * 1024 * 1024
for path in sources:
    assert path.stat().st_size <= 1024 * 1024, str(path)
recipes = list((playable / 'content/Common/CombatHelpers').glob('*.cs'))
assert len(recipes) == 9  # 01..08 de combate + 09_StatModifier
for path in recipes:
    source = path.read_text()
    assert '// Uso:' in source and '// Limite:' in source, str(path)
assert (playable / 'content/LEIA_PRIMEIRO.md').is_file()
sdk = ET.parse(root / 'experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj')
assert any('BunnyCombatUtilities.cs' in node.get('Include', '') for node in sdk.iter('Compile'))
compiler = (root / 'experiments/csharp/BunnySourceCompiler/Compiler.cs').read_text()
native = root / 'app/src/main/cpp/experiments/csharp'
assert 'MaxFiles = 4096;' in compiler
loader = (native / 'SourceLoader.cpp').read_text()
assert 'paths->size() >= kSourceMaxFiles' in loader and 'paths.size() > kSourceMaxFiles' in loader
cache = (native / 'AutoSourceRuntime.cpp').read_text()
assert 'static_cast<size_t>(++*count) > kSourceMaxFiles' in cache and 'static_cast<size_t>(*count) > kSourceMaxFiles' in cache
assert 'MaxTotalBytes = 32 * 1024 * 1024;' in compiler
limits=(native / 'SourceLimits.h').read_text()
assert 'kSourceMaxFiles=4096;' in limits and 'kSourceMaxTotalBytes=32*1024*1024;' in limits
assert 'kSourceMaxCategory=4096;' in limits and 'kSourceDefaultsBytes=8*1024*1024;' in limits
assert 'validSourceExecutedTypeCount(out.executedTypes)' in (native / 'ManagedRuntime.cpp').read_text()
print(f"PACKAGE_OK version={manifest['version']} sources={len(sources)} recipes={len(recipes)} limit=4096 guide=included")


