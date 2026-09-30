#!/usr/bin/env bash
set -euo pipefail

APK="${1:-}"
ROOT="${2:-.}"

if [ -z "$APK" ] || [ ! -f "$APK" ]; then
  echo "Uso: ./prepare_bunny_runtime.sh /caminho/Terraria.apk [pasta-do-Bunny]"
  exit 1
fi

JNI="$ROOT/app/src/main/jniLibs/arm64-v8a"
ASSETS="$ROOT/terraria1456_assets/src/main/assets"

mkdir -p "$JNI" "$ASSETS"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

unzip -q "$APK"   "lib/arm64-v8a/libil2cpp.so"   "lib/arm64-v8a/libunity.so"   "lib/arm64-v8a/libmain.so"   "lib/arm64-v8a/libc++_shared.so"   "assets/bin/Data/*"   -d "$tmp"

for lib in libil2cpp.so libunity.so libmain.so libc++_shared.so; do
  src="$tmp/lib/arm64-v8a/$lib"
  if [ ! -f "$src" ]; then
    echo "Faltando $lib"
    exit 1
  fi
  cp "$src" "$JNI/$lib"
done

if [ ! -d "$tmp/assets/bin/Data" ]; then
  echo "Faltando assets/bin/Data"
  exit 1
fi

mkdir -p "$ASSETS/bin"
rm -rf "$ASSETS/bin/Data"
cp -a "$tmp/assets/bin/Data" "$ASSETS/bin/Data"

META="$ASSETS/bin/Data/Managed/Metadata/global-metadata.dat"
if [ ! -f "$META" ]; then
  echo "Faltando global-metadata.dat"
  exit 1
fi

if strings "$JNI/libunity.so" | grep -q "libpairipcore.so"; then
  echo "Aviso: libunity.so referencia libpairipcore.so; esta build pode nao ser compativel."
else
  echo "libunity.so sem DT_NEEDED aparente para PairIP."
fi

echo "Runtime preparado:"
echo "  $JNI"
echo "  $ASSETS/bin/Data"
