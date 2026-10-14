"""Validate the example package and the shared source-loading limits before APK build."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
example = root / 'samples/ExampleModCSharp'
playable = example / 'Playable'
manifest = json.loads((playable / 'manifest.json').read_text())
compat = json.loads((example / 'compatibility.json').read_text())
assert manifest['version'] == compat['exampleModCSharpVersion'] == '0.1.34'
sources = sorted((playable / 'content').rglob('*.cs'))
assert len(sources) == 45, f'Expected 42 examples; found {len(sources)}'
assert sum(p.stat().st_size for p in sources) <= 2 * 1024 * 1024
for path in sources:
    assert path.stat().st_size <= 256 * 1024, str(path)
recipes = list((playable / 'content/Common/Recipes').glob('*.cs'))
assert len(recipes) == 8
for path in recipes:
    source = path.read_text()
    assert '// Uso:' in source and '// Limite:' in source, str(path)
assert (playable / 'content/LEIA_PRIMEIRO.md').is_file()
sdk = ET.parse(root / 'experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj')
assert any('BunnyCombatUtilities.cs' in node.get('Include', '') for node in sdk.iter('Compile'))
compiler = (root / 'experiments/csharp/BunnySourceCompiler/Compiler.cs').read_text()
native = root / 'app/src/main/cpp/experiments/csharp'
assert 'MaxFiles = 64;' in compiler
loader = (native / 'SourceLoader.cpp').read_text()
assert 'paths->size() >= 64' in loader and 'paths.size() > 64' in loader
cache = (native / 'AutoSourceRuntime.cpp').read_text()
assert '++*count > 64' in cache and '*count > 64' in cache
print('PACKAGE_OK version=0.1.34 sources=45 recipes=8 limit=64 guide=included')


