#!/usr/bin/env bash
# Package-only repair: reuse the already signed APK, publish no new APK.
set -euo pipefail
: "${PRIVATE_INPUTS_REPO:?}" "${GITHUB_SHA:?}" "${RUNNER_TEMP:?}"
TAG=five-loaders-qa-36944d9574de
APK=Bunny-Five-Loaders-QA-0.1.0.apk
APK_SHA=8f21158f998bc36b903f1d255faba08e9c39ffd2fc941be220715ae859baf557
APK_COMMIT=36944d9574de29f3c91e61403f02966ce57cf9e2
OUT="$RUNNER_TEMP/five-package-repair"
mkdir -p "$OUT" "$OUT/verify"
test "$(gh repo view "$PRIVATE_INPUTS_REPO" --json isPrivate --jq .isPrivate)" = true
# GitHub's recorded digest must match the APK downloaded and verified previously.
DIGEST=$(gh api "repos/$PRIVATE_INPUTS_REPO/releases/tags/$TAG" --jq '.assets[] | select(.name=="Bunny-Five-Loaders-QA-0.1.0.apk") | .digest')
test "$DIGEST" = "sha256:$APK_SHA"
gh release download "$TAG" --repo "$PRIVATE_INPUTS_REPO" --pattern COMMIT.txt --pattern PACKAGE.txt --pattern SIGNATURE.txt --pattern integration.json --dir "$OUT"
test "$(cat "$OUT/COMMIT.txt")" = "$APK_COMMIT"
python3 tests/five_loaders/package_import_test.py
python3 tools/mods/pack.py samples/FiveLoadersCSharp/Playable
MOD_VERSION=$(python3 -c 'import json; print(json.load(open("samples/FiveLoadersCSharp/Playable/manifest.json"))["version"])')
MOD="fiveloaderscsharp-$MOD_VERSION.bl"
cp "build/mods-packs/$MOD" "$OUT/"
cp samples/FiveLoadersCSharp/Playable/content/TESTE_NO_MOBILE.md "$OUT/"
printf '%s\n' "$GITHUB_SHA" > "$OUT/MOD-COMMIT.txt"
cat > "$OUT/NOTES.md" <<EOF
QA autorizado dos cinco loaders para Terraria Mobile 301720.

APK original: $APK_COMMIT
CI do APK: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/40039872270

Mod corrigido: $MOD
Fontes do mod: $GITHUB_SHA
Correção: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/146
CI da correção: https://github.com/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID

A versão 0.1.0 do mod declarava main.cs sem incluir content/main.cs. A versão $MOD_VERSION corrige a importação e mantém os cinco conteúdos. O APK não mudou: basta importar o novo .bl, sem reinstalar o aplicativo. Consulte TESTE_NO_MOBILE.md.

COMMIT.txt identifica o APK; MOD-COMMIT.txt identifica o mod corrigido. Gameplay no aparelho continua em teste.
EOF
(cd "$OUT" && sha256sum ./*.txt ./*.json ./*.md ./*.bl) > "$OUT/SHA256SUMS"
printf '%s  %s\n' "$APK_SHA" "$APK" >> "$OUT/SHA256SUMS"
# Verify the replacement on the server before removing the broken old mod.
gh release upload "$TAG" "$OUT/$MOD" --repo "$PRIVATE_INPUTS_REPO" --clobber
gh release download "$TAG" --repo "$PRIVATE_INPUTS_REPO" --pattern "$MOD" --dir "$OUT/verify"
cmp "$OUT/$MOD" "$OUT/verify/$MOD"
python3 - "$OUT/verify/$MOD" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, 'tools/mods')
from publish import Package
package = Package(Path(sys.argv[1]))
try:
    assert 'content/main.cs' in package.names
finally:
    package.zip.close()
print('PUBLISHED_CORRECTED_MOD_IMPORT_VALIDATION_PASS')
PY
gh release upload "$TAG" "$OUT/TESTE_NO_MOBILE.md" "$OUT/MOD-COMMIT.txt" "$OUT/NOTES.md" "$OUT/SHA256SUMS" --repo "$PRIVATE_INPUTS_REPO" --clobber
gh release edit "$TAG" --repo "$PRIVATE_INPUTS_REPO" --notes-file "$OUT/NOTES.md"
if gh release view "$TAG" --repo "$PRIVATE_INPUTS_REPO" --json assets --jq '.assets[].name' | rg -qx 'fiveloaderscsharp-0.1.0.bl'; then
    gh release delete-asset "$TAG" fiveloaderscsharp-0.1.0.bl --repo "$PRIVATE_INPUTS_REPO" --yes
fi
printf 'PACKAGE_ONLY_DELIVERY=https://github.com/%s/releases/tag/%s\n' "$PRIVATE_INPUTS_REPO" "$TAG"
