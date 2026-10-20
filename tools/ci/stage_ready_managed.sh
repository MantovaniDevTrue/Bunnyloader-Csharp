#!/usr/bin/env bash
set -euo pipefail
for project in BunnyManagedProbe BunnyVmBenchmark67 BunnyVmGameplay69 BunnySourceSdk BunnySourceCompiler; do
  dotnet build "experiments/csharp/$project/$project.csproj" -c Release --nologo
  cp "experiments/csharp/$project/bin/Release/net8.0/$project.dll" app/src/main/assets/managed/
done
for assembly in Microsoft.CodeAnalysis.dll Microsoft.CodeAnalysis.CSharp.dll; do
  cp "experiments/csharp/BunnySourceCompiler/bin/Release/net8.0/$assembly" app/src/main/assets/managed/
done
python3 - <<'PY'
from pathlib import Path
import hashlib
root=Path('app/src/main/assets/managed')
(root/'framework.sha256').write_text(''.join(f'{p.name}\t{hashlib.sha256(p.read_bytes()).hexdigest()}\n' for p in sorted(root.glob('*.dll'))))
PY
