from __future__ import annotations

import json
import sys
import zipfile
from pathlib import Path, PurePosixPath


def package(previous: Path, playable: Path, destination: Path) -> None:
    manifest = json.loads((playable / "manifest.json").read_text(encoding="utf-8"))
    source_dir = playable / "content"
    sources = sorted(source_dir.rglob("*.cs"))
    if len(sources) != 14:
        raise ValueError(f"Expected 14 C# files, found {len(sources)}")
    if manifest["id"] != "examplemodcsharp" or manifest["version"] != "0.1.8":
        raise ValueError("Unexpected ExampleModCSharp identity or version")
    with zipfile.ZipFile(previous) as original:
        old_manifest = json.loads(original.read("manifest.json"))
        if (old_manifest["id"], old_manifest["uid"]) != (manifest["id"], manifest["uid"]):
            raise ValueError("Original package identity mismatch")
        expected_assets = {
            "content/Content/Items/Weapons/SourceWand.png",
            "content/Content/Projectiles/SourceBolt.png",
        }
        if not expected_assets.issubset(set(original.namelist())):
            raise ValueError("Validated 0.1.7 textures are not available")
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_suffix(destination.suffix + ".tmp")
        added = set()
        try:
            with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as output:
                for entry in original.infolist():
                    name = entry.filename
                    path = PurePosixPath(name)
                    if path.is_absolute() or ".." in path.parts or "\\" in name:
                        raise ValueError(f"Invalid previous archive path: {name}")
                    if name == "manifest.json" or (
                        name.startswith("content/") and name.endswith(".cs")
                    ):
                        continue
                    if name in added:
                        raise ValueError(f"Duplicated archive path: {name}")
                    output.writestr(entry, original.read(entry))
                    added.add(name)
                output.writestr(
                    "manifest.json",
                    json.dumps(manifest, ensure_ascii=False, indent=2).encode("utf-8") + b"\n",
                )
                for source in sources:
                    path = "content/" + source.relative_to(source_dir).as_posix()
                    if path in added:
                        raise ValueError(f"Duplicate content file: {path}")
                    output.write(source, path)
                    added.add(path)
            with zipfile.ZipFile(temporary) as result:
                if result.testzip() is not None:
                    raise ValueError("Corrupted output ZIP")
                names = set(result.namelist())
                expected_sources = {
                    "content/" + source.relative_to(source_dir).as_posix()
                    for source in sources
                }
                if not expected_sources.issubset(names) or not expected_assets.issubset(names):
                    raise ValueError("Missing C# source or original sprite in generated package")
                if json.loads(result.read("manifest.json"))["version"] != "0.1.8":
                    raise ValueError("Invalid generated manifest version")
            temporary.replace(destination)
        finally:
            if temporary.exists():
                temporary.unlink()


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("Usage: package_dual_example.py OLD.bmod PLAYABLE_DIR OUT.bmod")
    package(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]))
    print("EXAMPLEMOD_CSHARP_018_PACKAGE=PASS")
