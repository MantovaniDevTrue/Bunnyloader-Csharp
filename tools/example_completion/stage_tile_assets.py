#!/usr/bin/env python3
"""Copia os sprites originais para o ExampleMod C#, sem recriar ou alterar pixels.

As fontes sao os PNG do ExampleModBunny ja incluido neste repositorio.
O SHA Git fixa exatamente os bytes do Jonata; falhas interrompem o lote.
Este script nao gera .bmod nem APK.
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASSETS = {
    "Content/Tiles/ExampleTile.png": "004130be8f7be70fb412f0862fe8f820aebd146f",
    "Content/Items/Placeable/ExampleTileItem.png": "a0e4fe1ca99fc64a73618f09b8b9800a5fab0b81",
}


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\x00" + data).hexdigest()


def stage(project_root: Path, copy: bool = False) -> list[str]:
    source = project_root / "samples/ExampleMod/content"
    target = project_root / "samples/ExampleModCSharp/Playable/content"
    errors = []
    for relative, expected in ASSETS.items():
        original = source / relative
        dest = target / relative
        if not original.is_file():
            errors.append(f"Ausente no original: {relative}")
            continue
        data = original.read_bytes()
        if git_blob(data) != expected:
            errors.append(f"Sprite alterado; use os bytes originais do Jonata: {relative}")
            continue
        if copy:
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(original, dest)
        if not dest.is_file() or dest.read_bytes() != data:
            errors.append(f"Sprite C# nao coincide com o original: {relative}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Conferir/copiar sprites originais, sem builds")
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--copy", action="store_true", help="Escrever apenas PNG identicos ao original")
    args = parser.parse_args()
    problems = stage(args.project_root, args.copy)
    for problem in problems:
        print(problem)
    if problems:
        return 1
    print("EXAMPLE_TILE_ASSETS=PASS: sprites binarios identicos ao upstream")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
