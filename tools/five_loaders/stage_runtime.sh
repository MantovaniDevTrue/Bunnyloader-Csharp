#!/usr/bin/env bash
set -euo pipefail
: "${RUNNER_TEMP:?}" "${PRIVATE_INPUTS_REPO:?}" "${RUNTIME_TAG:?}"
test "$(gh repo view "$PRIVATE_INPUTS_REPO" --json isPrivate --jq .isPrivate)" = true
mkdir -p "$RUNNER_TEMP/five-runtime"
gh release download "$RUNTIME_TAG" --repo "$PRIVATE_INPUTS_REPO" \
 --pattern BunnyRuntime_Terraria_1.4.5.8.6.zip --pattern unity-classes-2021.3.56f2-il2cpp-release.jar \
 --dir "$RUNNER_TEMP/five-runtime"
echo "2eb8d02dea144cb2fff6a27b94cc371e50ffd9aa44ba4dcd123337691a54c0ab  $RUNNER_TEMP/five-runtime/BunnyRuntime_Terraria_1.4.5.8.6.zip" | sha256sum -c -
echo "c1962b28afaeedba58427154e590f7b3537a98dfc426036456256d39f3385575  $RUNNER_TEMP/five-runtime/unity-classes-2021.3.56f2-il2cpp-release.jar" | sha256sum -c -
unzip -q "$RUNNER_TEMP/five-runtime/BunnyRuntime_Terraria_1.4.5.8.6.zip" -d "$RUNNER_TEMP/five-runtime/extracted"
for lib in libil2cpp.so libunity.so libmain.so libc++_shared.so; do
 cp "$RUNNER_TEMP/five-runtime/extracted/lib/arm64-v8a/$lib" app/src/main/jniLibs/arm64-v8a/
done
mkdir -p terraria1456_assets/src/main/assets/bin app/libs
cp -a "$RUNNER_TEMP/five-runtime/extracted/assets/bin/Data" terraria1456_assets/src/main/assets/bin/Data
cp "$RUNNER_TEMP/five-runtime/unity-classes-2021.3.56f2-il2cpp-release.jar" app/libs/unity-classes.jar
for project in BunnyManagedProbe BunnyVmBenchmark67 BunnyVmGameplay69 BunnySourceSdk BunnySourceCompiler; do
 dotnet build "experiments/csharp/$project/$project.csproj" -c Release --nologo -m:1
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

python3 tools/five_loaders/verify_mobile_binary.py app/src/main/jniLibs/arm64-v8a/libil2cpp.so

mkdir -p "$RUNNER_TEMP/five-sprites"
gh release download "$RUNTIME_TAG" --repo "$PRIVATE_INPUTS_REPO" --pattern BunnyLoader-v0.1.9.1-MusicPrewarm.apk --dir "$RUNNER_TEMP/five-sprites"
echo "aaf7c46046e776a966122d7f4e26469ebb72ad2fc21f0c6e57f98189e0c93c00  $RUNNER_TEMP/five-sprites/BunnyLoader-v0.1.9.1-MusicPrewarm.apk" | sha256sum -c -
unzip -q "$RUNNER_TEMP/five-sprites/BunnyLoader-v0.1.9.1-MusicPrewarm.apk" 'assets/sprites/*' -d "$RUNNER_TEMP/five-sprites"
mkdir -p _sprites
for kind in item npc buff; do cp -a "$RUNNER_TEMP/five-sprites/assets/sprites/$kind" _sprites/; done
