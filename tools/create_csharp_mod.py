#!/usr/bin/env python3
"""Create an independent source C# mod and an optional desktop editor project."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import uuid
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "samples/StarterModCSharp/Playable"


def create(mod_id: str, destination: Path, name: str, namespace: str | None = None) -> Path:
    if not re.fullmatch(r"[a-z][a-z0-9_]{2,47}", mod_id):
        raise ValueError("ID: use 3..48 lowercase letters, digits or underscores; start with a letter")
    namespace = namespace or "".join(part.capitalize() for part in mod_id.split("_"))
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*", namespace):
        raise ValueError("Invalid C# namespace")
    if not name.strip() or len(name) > 100 or any(ord(c) < 32 for c in name):
        raise ValueError("Invalid mod name")
    destination = destination.absolute()
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(f"Destination already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".bunny-starter-", dir=destination.parent) as temporary:
        staging = Path(temporary) / "mod"
        playable = staging / "Playable"
        shutil.copytree(TEMPLATE, playable)
        manifest_path = playable / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest.update(uid=str(uuid.uuid4()), id=mod_id, name=name)
        manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
        for path in (playable / "content").rglob("*.cs"):
            path.write_text(path.read_text().replace("namespace StarterModCSharp;", f"namespace {namespace};"))
        sdk = ROOT / "experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj"
        relative = escape(os.path.relpath(sdk, destination).replace(os.sep, "/"), {'"': '&quot;'})
        (staging / "Mod.csproj").write_text(
            '<Project Sdk="Microsoft.NET.Sdk">\n'
            '  <PropertyGroup>\n'
            '    <TargetFramework>net8.0</TargetFramework>\n'
            '    <Nullable>enable</Nullable>\n'
            '    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n'
            '  </PropertyGroup>\n'
            '  <ItemGroup>\n'
            '    <Compile Include="Playable/content/**/*.cs" />\n'
            f'    <ProjectReference Include="{relative}" />\n'
            '  </ItemGroup>\n'
            '</Project>\n')
        (staging / "README.md").write_text(
            f'# {name}\n\n'
            'Edit `Playable/content/Content/Items/StarterMaterial.cs`.\n\n'
            'Build `Mod.csproj` for desktop editor checks. Android compiles the sources automatically.\n'
            'Package only `Playable/` with the Bunny `tools/mods/pack.py` tool.\n'
            'Keep this mod UID stable between updates; generate a new mod for a different UID.\n')
        if destination.exists():
            raise FileExistsError(f"Destination appeared during generation: {destination}")
        staging.rename(destination)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("id")
    parser.add_argument("destination", type=Path)
    parser.add_argument("--name", required=True)
    parser.add_argument("--namespace")
    args = parser.parse_args()
    try:
        print(create(args.id, args.destination, args.name, args.namespace))
    except (ValueError, OSError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
