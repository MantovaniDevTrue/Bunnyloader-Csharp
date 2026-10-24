#!/usr/bin/env bash
set -euo pipefail
: "${PRIVATE_INPUTS_REPO:?}" "${GITHUB_SHA:?}" "${GITHUB_RUN_ID:?}" "${ANDROID_HOME:?}"
test "$(gh repo view "$PRIVATE_INPUTS_REPO" --json isPrivate --jq .isPrivate)" = true
APK=app/build/outputs/apk/debug/app-debug.apk
TOOLS="$ANDROID_HOME/build-tools/34.0.0"
mkdir -p build/unified-delivery
"$TOOLS/apksigner" verify --verbose "$APK" > build/unified-delivery/SIGNATURE.txt
grep -F 'Verified using v3 scheme (APK Signature Scheme v3): true' build/unified-delivery/SIGNATURE.txt
"$TOOLS/zipalign" -c 4 "$APK"
"$TOOLS/aapt" dump badging "$APK" > build/unified-delivery/PACKAGE.txt
grep -F "package: name='com.bunnyloader.csharptest'" build/unified-delivery/PACKAGE.txt
grep -F "versionCode='315'" build/unified-delivery/PACKAGE.txt
grep -F "versionName='1.0.14-csharp-unified-qa'" build/unified-delivery/PACKAGE.txt
python3 - <<'PY'
import hashlib,zipfile
from pathlib import Path
with zipfile.ZipFile('app/build/outputs/apk/debug/app-debug.apk') as z:
 names=set(z.namelist())
 for lib in ['libbunny.so','libbunnymonoprobe.so','libmonosgen-2.0.so','libbunnycompileraot.so','libil2cpp.so','libunity.so','libmain.so','libc++_shared.so']:
  assert 'lib/arm64-v8a/'+lib in names,lib
 assert any(n.startswith('assets/bin/Data/') for n in names)
 # AGP may rewrite ELF section/file offsets while stripping. Verify the
 # executable payload and metadata that define the reviewed Mobile ABI.
 mobile=z.read('lib/arm64-v8a/libil2cpp.so')
 assert hashlib.sha256(mobile[0x1000:0x2f55a34]).hexdigest() == 'b8e90015ebf283a6890e95682f047e10c76e6ca506fa09595ec5992f395a1b76'
 assert hashlib.sha256(z.read('assets/bin/Data/Managed/Metadata/global-metadata.dat')).hexdigest() == '4c904c49d168913d28ef547c4f461e539d926d2ed27c0253ed53a2954e9585f7'
 for line in z.read('assets/managed/framework.sha256').decode().splitlines():
  name,digest=line.split('\t');assert hashlib.sha256(z.read('assets/managed/'+name)).hexdigest()==digest,name
 sdk=z.read('assets/managed/BunnySourceSdk.dll')
 # Metadata strings verify that the newly built SDK, rather than the old
 # checked-in assembly, reached the APK. These checks are not gameplay QA.
 for symbol in [b'StatModifier',b'StatInheritanceData',b'BunnyWorldUpdateLoader',b'BunnyGore',b'BunnyWorldMonoHost',b'BunnyTouchButtonLoader']:
  assert symbol in sdk,symbol
 for symbol in [b'bunny_csharp_gore_spawn',b'bunny_csharp_touch_snapshot',b'bunny_system_time_rate_read',b'WorldSave']:
  assert symbol in z.read('lib/arm64-v8a/libbunny.so'),symbol
PY
python3 tests/five_loaders/package_import_test.py
python3 tools/mods/pack.py samples/FiveLoadersCSharp/Playable
python3 tools/mods/pack.py samples/ExampleModCSharp/Playable
cp "$APK" build/unified-delivery/Bunny-Loader-CSharp-Unified-QA-1.0.15.apk
MOD_VERSION=$(python3 -c 'import json; print(json.load(open("samples/FiveLoadersCSharp/Playable/manifest.json"))["version"])')
cp "build/mods-packs/fiveloaderscsharp-$MOD_VERSION.bl" build/unified-delivery/
EXAMPLE_VERSION=$(python3 -c 'import json; print(json.load(open("samples/ExampleModCSharp/Playable/manifest.json"))["version"])')
cp "build/mods-packs/examplemodcsharp-$EXAMPLE_VERSION.bl" build/unified-delivery/
cp docs/qa/UNIFIED_CSHARP_QA.md build/unified-delivery/
cp samples/FiveLoadersCSharp/Playable/content/TESTE_NO_MOBILE.md build/unified-delivery/
cp docs/qa/five-loaders-integration.json build/unified-delivery/integration.json
printf '%s\n' "$GITHUB_SHA" > build/unified-delivery/COMMIT.txt
(cd build/unified-delivery && sha256sum *.apk *.bl *.md COMMIT.txt PACKAGE.txt SIGNATURE.txt > SHA256SUMS)
TAG="csharp-unified-qa-${GITHUB_SHA:0:12}"
printf 'Bunny Loader CSharp unificado — QA Mobile 301720.\n\nCommit: %s\nCI: https://github.com/%s/actions/runs/%s\n\nUm APK para ExampleMod CSharp 0.1.65 e FiveLoadersCSharp 0.1.1. A compilacao Android e a assinatura foram verificadas. O SIGSEGV ao entrar no mundo ainda exige identificacao precisa do hook ou ponteiro; esta build serve ao teste supervisionado e a coleta de relatorio. Nao e release estavel. Leia UNIFIED_CSHARP_QA.md.\n' "$GITHUB_SHA" "$GITHUB_REPOSITORY" "$GITHUB_RUN_ID" > build/unified-delivery/NOTES.md
if gh release view "$TAG" --repo "$PRIVATE_INPUTS_REPO" >/dev/null 2>&1; then
 gh release upload "$TAG" build/unified-delivery/* --repo "$PRIVATE_INPUTS_REPO" --clobber
else
 gh release create "$TAG" build/unified-delivery/* --repo "$PRIVATE_INPUTS_REPO" --prerelease \
  --title 'Bunny Loader CSharp Unificado 1.0.15 — QA Mobile' --notes-file build/unified-delivery/NOTES.md
fi
printf 'PRIVATE_CSHARP_UNIFIED_QA=https://github.com/%s/releases/tag/%s\n' "$PRIVATE_INPUTS_REPO" "$TAG"
