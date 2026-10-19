#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACKAGE="$ROOT/tools/mono/prebuilt/arm64-v8a"
LIBRARY="$PACKAGE/libmonosgen-2.0.so"
DEST="$ROOT/app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so"

if [[ ! -s "$LIBRARY" || ! -s "$PACKAGE/libmonosgen-2.0.so.sha256" ]]; then
  echo "Biblioteca Mono modificada nao encontrada. Veja tools/mono/README.md." >&2
  exit 1
fi
if ! command -v readelf >/dev/null 2>&1; then
  echo "readelf (binutils) nao encontrado." >&2
  exit 1
fi
(
  cd "$PACKAGE"
  sha256sum -c libmonosgen-2.0.so.sha256
)
HEADER="$(readelf -h "$LIBRARY")"
if [[ "$HEADER" != *AArch64* ]]; then
  echo "Biblioteca nao e Android ARM64/AArch64." >&2
  exit 1
fi
SYMBOLS="$(readelf -Ws "$LIBRARY")"
for symbol in mono_bunny_android_signal_policy_v1 mono_jit_init mono_thread_attach; do
  if ! grep -F "$symbol" <<< "$SYMBOLS" >/dev/null; then
    echo "Simbolo obrigatorio ausente: $symbol" >&2
    exit 1
  fi
done
mkdir -p "$(dirname "$DEST")"
cp -f "$LIBRARY" "$DEST"
cmp "$LIBRARY" "$DEST"
echo "BUNNY_PATCHED_MONO_ARM64_STAGED=YES"
echo "$DEST"
echo "O framework Mono 8.0.23 e os assemblies devem ser preparados separadamente."
echo "Apos preparar o runtime oficial, execute este script por ULTIMO."
