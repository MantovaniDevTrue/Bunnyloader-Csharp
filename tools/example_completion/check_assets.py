#!/usr/bin/env python3
"""Verifica a integridade binaria dos assets originais do ExampleModBunny.

Nao interpreta imagens, nao reencoda audio, nao produz APK. Cada SHA Git
e do objeto 'blob <size>\\0<bytes>' do repositorio oficial do Jonata.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "tools/example_completion/current_upstream.json"
CONTENT = ROOT / "samples/ExampleModCSharp/Playable/content"


def blob_hash(path: Path) -> str:
    data = path.read_bytes()
    header = b"blob " + str(len(data)).encode("ascii") + b"\0"
    return hashlib.sha1(header + data).hexdigest()


def check(root: Path, manifest: dict) -> list[str]:
    errors = []
    assets = manifest.get("assets")
    if not isinstance(assets, list) or not assets:
        return ["Manifesto sem lista de arquivos originais"]
    if len(assets) != len({x.get("path") for x in assets if isinstance(x, dict)}):
        errors.append("Assets duplicados no inventario")
    base = (root / "samples/ExampleModCSharp/Playable/content").resolve()
    for entry in assets:
        if not isinstance(entry, dict):
            errors.append("Entrada de asset invalida")
            continue
        rel = entry.get("path")
        expected = entry.get("gitBlob")
        if not isinstance(rel, str) or not rel or "\\" in rel or (
            PurePosixPath(rel).is_absolute() or ".." in PurePosixPath(rel).parts
        ):
            errors.append(f"Caminho de asset inseguro: {rel}")
            continue
        target = (base / rel).resolve()
        if not target.is_relative_to(base) or not target.is_file():
            errors.append(f"Faltando no pacote C#: {rel}")
            continue
        if blob_hash(target) != expected:
            errors.append(f"Asset difere do original: {rel}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Verificacao completa de assets, sem APK")
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--inventory", type=Path, default=INVENTORY)
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text())
    errors = check(args.project_root, inventory)
    if errors:
        print(f"EXAMPLEMOD_SOURCE_ASSETS=BLOCKED: {len(errors)}")
        for issue in errors[:30]:
            print(" - " + issue)
        return 1
    print(f"EXAMPLEMOD_SOURCE_ASSETS=PASS: {len(inventory['assets'])} assets identicos, nenhum APK gerado")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
