#!/usr/bin/env bash
set -euo pipefail
: "${RUNNER_TEMP:?}" "${PRIVATE_INPUTS_REPO:?}" "${RUNTIME_TAG:?}"
test "$(gh repo view "$PRIVATE_INPUTS_REPO" --json isPrivate --jq '.isPrivate')" = true
mkdir -p "$RUNNER_TEMP/ready-runtime"
gh release download "$RUNTIME_TAG" --repo "$PRIVATE_INPUTS_REPO" --pattern BunnyRuntime_Terraria_1.4.5.8.6.zip --dir "$RUNNER_TEMP/ready-runtime"
echo "2eb8d02dea144cb2fff6a27b94cc371e50ffd9aa44ba4dcd123337691a54c0ab  $RUNNER_TEMP/ready-runtime/BunnyRuntime_Terraria_1.4.5.8.6.zip" | sha256sum -c -
unzip -q "$RUNNER_TEMP/ready-runtime/BunnyRuntime_Terraria_1.4.5.8.6.zip" -d "$RUNNER_TEMP/ready-runtime/extracted"
for lib in libil2cpp.so libunity.so libmain.so libc++_shared.so; do
  cp "$RUNNER_TEMP/ready-runtime/extracted/lib/arm64-v8a/$lib" app/src/main/jniLibs/arm64-v8a/
done
mkdir -p terraria1456_assets/src/main/assets/bin
cp -a "$RUNNER_TEMP/ready-runtime/extracted/assets/bin/Data" terraria1456_assets/src/main/assets/bin/Data
bash tools/android/stage_unity_classes.sh
mkdir -p "$RUNNER_TEMP/ready-sprites"
gh release download "$RUNTIME_TAG" --repo "$PRIVATE_INPUTS_REPO" --pattern BunnyLoader-v0.1.9.1-MusicPrewarm.apk --dir "$RUNNER_TEMP/ready-sprites"
echo "aaf7c46046e776a966122d7f4e26469ebb72ad2fc21f0c6e57f98189e0c93c00  $RUNNER_TEMP/ready-sprites/BunnyLoader-v0.1.9.1-MusicPrewarm.apk" | sha256sum -c -
unzip -q "$RUNNER_TEMP/ready-sprites/BunnyLoader-v0.1.9.1-MusicPrewarm.apk" 'assets/sprites/*' -d "$RUNNER_TEMP/ready-sprites"
mkdir -p _sprites
for kind in item npc buff; do cp -a "$RUNNER_TEMP/ready-sprites/assets/sprites/$kind" _sprites/; done
