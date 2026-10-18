#!/usr/bin/env python3
import argparse
import hashlib
import shutil
from pathlib import Path

NEW_DIRS = (
    "app/src/main/cpp/experiments/csharp",
    "experiments/csharp/BunnyCsSdk",
    "experiments/csharp/BunnySourceSdk",
    "experiments/csharp/BunnySourceCompiler",
    "experiments/csharp/BunnySourceCompiler.Smoke",
    "experiments/csharp/BunnyManagedProbe",
    "experiments/csharp/BunnyVmBenchmark67",
    "experiments/csharp/BunnyVmGameplay69",
    "tools/mono",
)

NEW_FILES = (
    "app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt",
    "app/src/main/java/dev/bunnyloader/game/LogcatDiagnostics.java",
    "app/src/main/java/net/dot/android/crypto/DotnetProxyTrustManager.java",
    "app/src/main/java/com/unity3d/player/UnityJniClasses.java",
    "app/src/main/cpp/content/items/ItemRegistry.cpp",
    "app/src/main/cpp/content/items/ItemRegistryInternal.h",
    "app/src/main/cpp/content/recipes/NativeRecipes.cpp",
    "app/src/main/cpp/content/recipes/NativeRecipes.h",
    "app/src/main/cpp/content/sounds/NativeMusicRouter.cpp",
    "app/src/main/cpp/content/sounds/NativeMusicRouter.h",
    "app/src/main/cpp/content/sounds/NativeSoundRouter.cpp",
    "app/src/main/cpp/content/sounds/NativeSoundRouter.h",
    "app/src/main/cpp/content/tiles/TileRegistry.cpp",
    "app/src/main/cpp/content/tiles/TileRegistryInternal.h",
    "app/src/main/cpp/il2cpp/ValueLayout.h",
    "app/src/main/cpp/script/api/CSharpPrototypeApi.cpp",
    "app/src/main/cpp/script/api/CSharpPrototypeApi.h",
    "tools/stage_mono_runtime.py",
)

REVIEW_FILES = (
    "app/build.gradle.kts",
    "app/src/main/AndroidManifest.xml",
    "app/src/main/cpp/CMakeLists.txt",
    "app/src/main/cpp/boot/Probe.cpp",
    "app/src/main/cpp/boot/QuickStart.cpp",
    "app/src/main/cpp/boot/jni_entry.cpp",
    "app/src/main/cpp/content/common/TypeTables.cpp",
    "app/src/main/cpp/content/common/TypeTables.h",
    "app/src/main/cpp/content/items/ModItemSave.cpp",
    "app/src/main/cpp/content/items/ModItemSave.h",
    "app/src/main/cpp/content/items/ModItems.cpp",
    "app/src/main/cpp/content/npcs/ModNpcs.cpp",
    "app/src/main/cpp/content/npcs/ModNpcs.h",
    "app/src/main/cpp/content/projectiles/ModProjectiles.cpp",
    "app/src/main/cpp/content/projectiles/ModProjectiles.h",
    "app/src/main/cpp/content/sounds/AndroidAudio.cpp",
    "app/src/main/cpp/content/sounds/AndroidAudio.h",
    "app/src/main/cpp/content/tiles/ModTileSave.cpp",
    "app/src/main/cpp/content/tiles/ModTiles.cpp",
    "app/src/main/cpp/content/tiles/ModTiles.h",
    "app/src/main/cpp/core/Config.h",
    "app/src/main/cpp/core/Log.cpp",
    "app/src/main/cpp/il2cpp/Api.cpp",
    "app/src/main/cpp/il2cpp/Api.h",
    "app/src/main/cpp/menu/CheatButton.cpp",
    "app/src/main/cpp/menu/Cheats.cpp",
    "app/src/main/cpp/mods/ModLoader.cpp",
    "app/src/main/cpp/script/api/Sounds.cpp",
    "app/src/main/cpp/script/bridge/Bindings.cpp",
    "app/src/main/cpp/script/js/ModHelpers.js",
    "app/src/main/cpp/script/js/mod/Exports.js",
    "app/src/main/cpp/script/js/mod/Loaders/ArmorSetLoader.js",
    "app/src/main/cpp/script/js/mod/Loaders/EquipLoader.js",
    "app/src/main/cpp/script/js/mod/Loaders/ItemLoader.js",
    "app/src/main/cpp/script/js/mod/Loaders/ModMusic.js",
    "app/src/main/cpp/script/js/mod/Loaders/RecipeLoader.js",
    "app/src/main/cpp/script/js/mod/Loaders/SoundLoader.js",
    "app/src/main/cpp/script/js/mod/Loaders/VanillaMusic.js",
    "app/src/main/cpp/script/js/mod/Loaders/VanillaTooltips.js",
    "app/src/main/cpp/script/js/mod/ModItem.js",
    "app/src/main/cpp/script/js/mod/SoundEngine.js",
    "app/src/main/cpp/script/js/mod/SoundStyle.js",
    "app/src/main/kotlin/dev/bunnyloader/GameActivity.kt",
    "app/src/main/kotlin/dev/bunnyloader/game/BootLog.kt",
    "app/src/main/kotlin/dev/bunnyloader/game/ModAudio.kt",
    "app/src/main/kotlin/dev/bunnyloader/mods/ModRepository.kt",
    "app/src/main/kotlin/dev/bunnyloader/nativebridge/NativeBridge.kt",
    "app/src/main/res/values/strings.xml",
)

IGNORED = {"bin", "obj", ".gradle", ".git", ".vs", "__pycache__"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def directory_digest(path):
    entries = []
    for file in sorted(path.rglob("*")):
        if any(part in IGNORED for part in file.relative_to(path).parts):
            continue
        if file.is_file():
            entries.append((file.relative_to(path).as_posix(), digest(file)))
    return entries


def same(src, dst):
    if src.is_file() and dst.is_file():
        return digest(src) == digest(dst)
    if src.is_dir() and dst.is_dir():
        return directory_digest(src) == directory_digest(dst)
    return False


def main():
    parser = argparse.ArgumentParser(description="Plano seguro para levar o C# ao Bunny original")
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1] / "Split/Bunny-Loader")
    parser.add_argument("--target", type=Path, required=True, help="Raiz do clone original Bunny-Loader")
    parser.add_argument("--apply-additions", action="store_true", help="Copia somente caminhos inteiramente ausentes, nunca sobrescreve")
    args = parser.parse_args()

    src_root = args.source.resolve()
    dst_root = args.target.resolve()
    if src_root == dst_root or not (src_root / "app/src/main/cpp/CMakeLists.txt").is_file():
        parser.error("--source deve apontar para Split/Bunny-Loader do port C#")
    if not (dst_root / "app/src/main/cpp/CMakeLists.txt").is_file():
        parser.error("--target deve apontar para a raiz de um clone do Bunny Loader original")

    additions = []
    collisions = []
    missing = []
    for path in (*NEW_DIRS, *NEW_FILES):
        src, dst = src_root / path, dst_root / path
        if not src.exists():
            missing.append(path)
            print(f"MISSING_SOURCE  {path}")
        elif dst.exists() or dst.is_symlink():
            if same(src, dst):
                print(f"ALREADY_IDENTICAL  {path}")
            else:
                collisions.append(path)
                print(f"COLLISION_REVIEW   {path}")
        else:
            additions.append((src, dst, path))
            print(f"ADD_NEW            {path}")

    review_pending = 0
    for path in REVIEW_FILES:
        src, dst = src_root / path, dst_root / path
        if not src.is_file():
            missing.append(path)
            print(f"MISSING_SOURCE  {path}")
        elif not dst.is_file():
            review_pending += 1
            print(f"MISSING_ANCHOR  {path}")
        elif same(src, dst):
            print(f"ALREADY_IDENTICAL  {path}")
        else:
            review_pending += 1
            print(f"MERGE_MANUALLY     {path}")

    print(f"\nAdditions ready={len(additions)}; collisions={len(collisions)}; source missing={len(missing)}; manual reviews={review_pending}")
    if missing or collisions:
        raise SystemExit("Abortado: resolva fontes ausentes ou conflitos antes de qualquer cópia")
    if args.apply_additions:
        for src, dst, path in additions:
            dst.parent.mkdir(parents=True, exist_ok=True)
            if src.is_dir():
                shutil.copytree(src, dst, ignore=shutil.ignore_patterns(*IGNORED))
            else:
                shutil.copy2(src, dst)
            print(f"COPIED_NEW         {path}")
        print("Adições copiadas. Os MERGE_MANUALLY continuam pendentes; integração não concluída.")
    else:
        print("Simulação somente leitura. Use --apply-additions apenas depois de revisar o plano.")
    print("Nunca copie os diretórios completos app/, script/, Kotlin/ ou assets gerados por cima do upstream.")


if __name__ == "__main__":
    main()
