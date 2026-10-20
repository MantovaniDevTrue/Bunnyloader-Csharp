#!/usr/bin/env bash
set -euo pipefail

ROOT="${GITHUB_WORKSPACE:-$(pwd)}"
DEST="$ROOT/app/libs/unity-classes.jar"
VERSION="2021.3.56f2"
ASSET="unity-classes-${VERSION}-il2cpp-release.jar"
EXPECTED="c1962b28afaeedba58427154e590f7b3537a98dfc426036456256d39f3385575"
PRIVATE="${PRIVATE_INPUTS_REPO:?Missing private input repository}"
TAG="${RUNTIME_TAG:?Missing private input release tag}"
TEMP="$(mktemp -d)"
trap 'rm -rf "$TEMP"' EXIT

mkdir -p "$(dirname "$DEST")"
rm -f "$DEST"

cached=0
if gh release download "$TAG" --repo "$PRIVATE" --pattern "$ASSET" --dir "$TEMP" >/dev/null 2>&1; then
    cp "$TEMP/$ASSET" "$DEST"
    cached=1
else
    URL="https://download.unity3d.com/download_unity/667925324b48/MacEditorTargetInstaller/UnitySetup-Android-Support-for-Editor-2021.3.56f2.pkg"
    curl --fail --location --show-error --silent --retry 4 --retry-all-errors \
        --connect-timeout 45 --max-time 2400 "$URL" --output "$TEMP/unity.pkg"

    if ! command -v bsdtar >/dev/null || ! command -v cpio >/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq libarchive-tools cpio
    fi

    mkdir -p "$TEMP/unpacked"
    bsdtar -xf "$TEMP/unity.pkg" -C "$TEMP/unpacked"

    if ! find "$TEMP/unpacked" -type f -name Payload -print -quit | grep -q .; then
        while IFS= read -r -d '' nested; do
            dir="$TEMP/nested-$(basename "$nested" | tr -cd '[:alnum:]_-')"
            mkdir -p "$dir"
            bsdtar -xf "$nested" -C "$dir"
        done < <(find "$TEMP/unpacked" -type f -name '*.pkg' -print0)
    fi

    found=0
    while IFS= read -r -d '' payload; do
        if gzip -dc "$payload" | cpio -i --quiet --to-stdout \
            '*Variations/il2cpp/Release/Classes/classes.jar' > "$TEMP/candidate.jar"; then
            if [ -s "$TEMP/candidate.jar" ]; then
                mv "$TEMP/candidate.jar" "$DEST"
                found=1
                break
            fi
        fi
    done < <(find "$TEMP/unpacked" "$TEMP" -type f -name Payload -print0 2>/dev/null)
    if [ "$found" -ne 1 ]; then
        echo "::error::Unity 2021.3.56f2 il2cpp/Release/Classes/classes.jar not found in official installer"
        exit 1
    fi
fi

echo "$EXPECTED  $DEST" | sha256sum -c -
for class in UnityPlayer NativeLoader IUnityPlayerLifecycleEvents; do
    unzip -Z1 "$DEST" | grep -Fx "com/unity3d/player/$class.class" >/dev/null
done
echo "BUNNY_UNITY_2021_3_56F2_JAR_VERIFIED=PASS"

if [ "$cached" -eq 0 ]; then
    cp "$DEST" "$TEMP/$ASSET"
    gh release upload "$TAG" "$TEMP/$ASSET" --repo "$PRIVATE" >/dev/null 2>&1 || true
fi
