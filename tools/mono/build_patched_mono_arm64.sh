#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUTPUT="${1:-$ROOT/out/mono-android-arm64-8.0.23}"
if [[ "$OUTPUT" != /* ]]; then
  OUTPUT="$(pwd)/$OUTPUT"
fi

NDK="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-}}"
if [[ -z "$NDK" && -n "${ANDROID_HOME:-}" ]]; then
  NDK="$ANDROID_HOME/ndk/30.0.16248370"
fi
if [[ -z "$NDK" || ! -f "$NDK/source.properties" ]]; then
  echo "Instale NDK 30.0.16248370 ou defina ANDROID_NDK_ROOT corretamente." >&2
  exit 2
fi
if [[ -z "${ANDROID_SDK_ROOT:-}" && -n "${ANDROID_HOME:-}" ]]; then
  export ANDROID_SDK_ROOT="$ANDROID_HOME"
fi
if [[ -z "${ANDROID_SDK_ROOT:-}" ]]; then
  echo "ANDROID_SDK_ROOT/ANDROID_HOME nao definido." >&2
  exit 2
fi
for tool in git python3 readelf dotnet; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Ferramenta ausente: $tool" >&2
    exit 2
  fi
done

mkdir -p "$OUTPUT"
BUILD_ROOT="${BL_MONO_BUILD_ROOT:-$(mktemp -d)}"
SRC="$BUILD_ROOT/dotnet-runtime"
if [[ -e "$SRC" ]]; then
  echo "Pasta temporaria ja ocupada: $SRC" >&2
  exit 2
fi
echo "MONO_SOURCE=dotnet/runtime@v8.0.23"
echo "MONO_OUTPUT=$OUTPUT"
echo "ANDROID_NDK_ROOT=$NDK"
git clone --filter=blob:none --depth 1 --branch v8.0.23 \
  https://github.com/dotnet/runtime.git "$SRC"

TARGET="$SRC/src/mono/mono/utils/mono-threads-posix-signals.c"
python3 "$ROOT/tools/mono/patch_mono_android_signals.py" "$TARGET"
python3 "$ROOT/tests/test_mono_android_signals_patch.py" "$TARGET"

export ANDROID_NDK_ROOT="$NDK"
export ANDROID_NDK_HOME="$NDK"
(
  cd "$SRC"
  ./build.sh mono -os android -arch arm64 -c Release \
    -p:MonoEnableLLVM=false \
    -p:BuildMonoAOTCrossCompiler=false \
    -p:DisableCrossgen=true
)

mapfile -t CANDIDATES < <(find "$SRC/artifacts" -type f -name libmonosgen-2.0.so)
if (( ${#CANDIDATES[@]} == 0 )); then
  echo "A compilacao nao gerou libmonosgen-2.0.so." >&2
  exit 1
fi

FOUND=""
for candidate in "${CANDIDATES[@]}"; do
  if readelf -h "$candidate" | grep -q AArch64 &&
     readelf -Ws "$candidate" | grep -q mono_bunny_android_signal_policy_v1 &&
     readelf -Ws "$candidate" | grep -q mono_jit_init &&
     readelf -Ws "$candidate" | grep -q mono_thread_attach; then
    FOUND="$candidate"
    break
  fi
done
if [[ -z "$FOUND" ]]; then
  echo "Nao encontrei Mono ARM64 com o simbolo de sinais e as APIs obrigatorias." >&2
  exit 1
fi

cp "$FOUND" "$OUTPUT/libmonosgen-2.0.so"
if [[ -f "$SRC/LICENSE.TXT" ]]; then
  cp "$SRC/LICENSE.TXT" "$OUTPUT/DOTNET_RUNTIME_LICENSE.txt"
fi
(
  cd "$OUTPUT"
  sha256sum libmonosgen-2.0.so > libmonosgen-2.0.so.sha256
  sha256sum -c libmonosgen-2.0.so.sha256
)
echo "MONO_BUNNY_ANDROID_SIGNAL_POLICY=PASS"
echo "MONO_ARM64_LIBRARY=$OUTPUT/libmonosgen-2.0.so"
echo "Nao empacota arquivos do Terraria/Unity nem segredos."
echo "Para inserir no APK, copie para app/src/main/jniLibs/arm64-v8a/,"
echo "depois dos arquivos oficiais do runtime terem sido preparados."
