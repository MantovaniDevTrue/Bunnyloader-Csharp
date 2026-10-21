#!/usr/bin/env bash
set -euo pipefail
: "${PRIVATE_INPUTS_REPO:?}" "${GITHUB_SHA:?}" "${GITHUB_RUN_ID:?}"
test "$(gh repo view "$PRIVATE_INPUTS_REPO" --json isPrivate --jq .isPrivate)" = true
APK=app/build/outputs/apk/debug/app-debug.apk
TOOLS="$ANDROID_HOME/build-tools/34.0.0"
"$TOOLS/apksigner" verify --verbose "$APK" | tee build/ready-apk-signature.txt
grep -F "Verified using v3 scheme (APK Signature Scheme v3): true" build/ready-apk-signature.txt
"$TOOLS/zipalign" -c 4 "$APK"
"$TOOLS/aapt" dump badging "$APK" > build/ready-badging.txt
grep -F "package: name='com.bunnyloader.csharptest'" build/ready-badging.txt
grep -F "application-label:'Bunny Loader C# Test'" build/ready-badging.txt
python3 - <<'PY'
import hashlib,json,zipfile
from pathlib import Path
results=json.loads(Path('build/ready-pr-tests/results.json').read_text())
assert results and all(r['code']==0 for r in results)
with zipfile.ZipFile('app/build/outputs/apk/debug/app-debug.apk') as z:
    names=set(z.namelist())
    for lib in ['libbunny.so','libbunnymonoprobe.so','libmonosgen-2.0.so','libbunnycompileraot.so','libil2cpp.so','libunity.so','libmain.so']:
        assert 'lib/arm64-v8a/'+lib in names,lib
    assert any(n.startswith('assets/bin/Data/') for n in names)
    sdk=z.read('assets/managed/BunnySourceSdk.dll')
    assert b'BunnySounds' in sdk and b'BunnyCommandWorker' in sdk
    assert 'assets/managed/BunnySourceCompiler.dll' in names
    for line in z.read('assets/managed/framework.sha256').decode().splitlines():
        name,digest=line.split('\t');assert hashlib.sha256(z.read('assets/managed/'+name)).hexdigest()==digest,name
    for name in ['bunny_dust_emit','bunny_item_use_bind','bunny_item_use_poll_context','bunny_csharp_sound_register','bunny_csharp_sound_play','CommandLoader','Localization','Rarity','AnglerQuest','Invasion']:
        assert name.encode() in z.read('lib/arm64-v8a/libbunny.so'),name
PY
mkdir -p build/ready-delivery
cp "$APK" build/ready-delivery/BunnyLoader-CSharp-ReadyPRs-QA.apk
python3 tools/mods/pack.py samples/ExampleModCSharp/Playable
cp build/mods-packs/examplemodcsharp-0.1.63.bl build/ready-delivery/ExampleMod-CSharp-0.1.63.bl
cp docs/ready-prs-android-qa.md build/ready-delivery/TESTES-ANDROID.md
cp docs/ready-prs-integration.json build/ready-delivery/integration.json
cp build/ready-pr-tests/results.json build/ready-delivery/host-tests.json
printf '%s\n' "$GITHUB_SHA" > build/ready-delivery/COMMIT.txt
(cd build/ready-delivery && sha256sum *.apk *.bl *.md *.json COMMIT.txt > SHA256SUMS)
TAG="ready-prs-qa-${GITHUB_SHA:0:12}"
printf 'Build experimental dos PRs #101, #102, #114, #115, #116, #117 e #118.\n\nCommit: %s\nCI: https://github.com/%s/actions/runs/%s\n\nAPK assinado V3; ExampleMod editável. Android e multiplayer aguardam validação real. Não é release estável nem conclusão global dos loaders. Consulte TESTES-ANDROID.md.\n' "$GITHUB_SHA" "$GITHUB_REPOSITORY" "$GITHUB_RUN_ID" > build/ready-delivery/NOTES.md
if gh release view "$TAG" --repo "$PRIVATE_INPUTS_REPO" >/dev/null 2>&1; then
  gh release upload "$TAG" build/ready-delivery/* --repo "$PRIVATE_INPUTS_REPO" --clobber
else
  gh release create "$TAG" build/ready-delivery/* --repo "$PRIVATE_INPUTS_REPO" --prerelease --title 'Bunny C# — sete PRs integrados para QA Android' --notes-file build/ready-delivery/NOTES.md
fi
printf 'PRIVATE_PREVIEW=https://github.com/%s/releases/tag/%s\n' "$PRIVATE_INPUTS_REPO" "$TAG"
