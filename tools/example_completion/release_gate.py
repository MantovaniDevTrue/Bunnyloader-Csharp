#!/usr/bin/env python3
"""Bloqueia APKs enquanto algum exemplo do ExampleModBunny nao estiver completo.

A entrada e a matriz produzida por tools/example_completion_audit.py.
Compilar um C# nao prova fidelidade: cada arquivo original exige sua fonte,
teste automatizado, verificacao no Terraria Android e nenhuma pendencia.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath

UPSTREAM_COMMIT = "a0132963026e3b35e45f33ffffa01a75eb8535d5"
BASELINE_JS_COUNT = 206
COMPLETE = "Validado no Android"
ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MATRIX = ROOT / "docs/generated/example_completion/matrix.json"


def _relative_file(value: str, root: Path, extension: str) -> bool:
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    rel = PurePosixPath(value)
    if rel.is_absolute() or ".." in rel.parts or rel.suffix != extension:
        return False
    path = (root / value).resolve()
    return path.is_relative_to(root.resolve()) and path.is_file()


def check_release(matrix: dict, project: Path, upstream: Path | None,
                  upstream_commit: str = UPSTREAM_COMMIT) -> list[str]:
    """Rejeita ausencias sem promover automaticamente estados da matriz."""
    errors: list[str] = []
    references = matrix.get("references", {})
    revision = references.get("upstream", {}).get("commit")
    if revision != upstream_commit:
        errors.append(
            f"Referencia JS desatualizada: matriz={revision}, esperado={upstream_commit}"
        )

    rows = matrix.get("examples")
    if not isinstance(rows, list):
        return errors + ["Matriz sem lista de exemplos"]
    expected_count = matrix.get("counts", {}).get("canonicalJsFiles")
    if expected_count != BASELINE_JS_COUNT or len(rows) != BASELINE_JS_COUNT:
        errors.append(
            f"Inventario incompleto: matriz={expected_count}, linhas={len(rows)}, "
            f"minimo obrigatorio={BASELINE_JS_COUNT}"
        )
    ids: set[str] = set()
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            errors.append(f"Linha {index}: exemplo invalido")
            continue
        name = row.get("id")
        if not isinstance(name, str) or not name.endswith(".js"):
            errors.append(f"Linha {index}: caminho JS invalido")
            continue
        source_identity = row.get("javaScript", {})
        blob = source_identity.get("upstreamBlob") if isinstance(source_identity, dict) else None
        if not isinstance(blob, str) or len(blob) != 40 or any(c not in "0123456789abcdef" for c in blob):
            errors.append(f"{name}: identidade do arquivo original sem SHA Git verificavel")
        if name in ids:
            errors.append(f"{name}: arquivo JS duplicado")
        ids.add(name)
        if row.get("status") != COMPLETE:
            errors.append(f"{name}: {row.get('status', 'sem estado')}")
        if row.get("missingFeatures"):
            errors.append(f"{name}: recursos originais ainda faltam")
        if row.get("technicalBlockEvidence"):
            errors.append(f"{name}: possui bloqueio tecnico aberto")
        if not row.get("automatedEvidence") and not row.get("automatedTests"):
            errors.append(f"{name}: sem evidencia de teste automatizado")
        if not row.get("deviceEvidence"):
            errors.append(f"{name}: sem evidencia Android do comportamento integral")
        paths = row.get("cSharp")
        if not isinstance(paths, list) or not paths:
            errors.append(f"{name}: sem implementacao C#")
        else:
            for source in paths:
                if not _relative_file(source, project, ".cs"):
                    errors.append(f"{name}: fonte C# ausente ou caminho invalido: {source}")

    if upstream is None:
        errors.append("Checkout atual do upstream obrigatorio (--upstream-root)")
    else:
        source_root = upstream / "samples/ExampleMod/content"
        actual = {
            p.relative_to(source_root).as_posix()
            for p in source_root.rglob("*.js")
        } if source_root.is_dir() else set()
        if not actual:
            errors.append("Checkout upstream inexistente ou sem arquivos JS")
        for row in rows:
            if not isinstance(row, dict):
                continue
            name = row.get("id")
            if not isinstance(name, str) or name not in actual:
                continue
            source = source_root / name
            data = source.read_bytes()
            expected = row.get("javaScript", {}).get("upstreamBlob")
            actual_blob = hashlib.sha1(
                b"blob " + str(len(data)).encode("ascii") + b"\\0" + data
            ).hexdigest()
            if expected != actual_blob:
                errors.append(f"{name}: JS original mudou desde o inventario Git")
        if actual != ids:
            errors.append(
                f"Inventario diverge do upstream: +{len(actual - ids)} novos, "
                f"-{len(ids - actual)} sem correspondente"
            )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Gate integral ExampleMod C#; nao compila APK")
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--upstream-root", type=Path, required=True)
    parser.add_argument("--upstream-commit", default=UPSTREAM_COMMIT)
    args = parser.parse_args()
    try:
        report = json.loads(args.matrix.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"EXAMPLEMOD_APK_GATE=BLOCKED: matriz invalida: {error}")
        return 1
    problems = check_release(report, args.project_root, args.upstream_root,
                             args.upstream_commit)
    if problems:
        print(f"EXAMPLEMOD_APK_GATE=BLOCKED: {len(problems)} pendencias")
        for problem in problems[:30]:
            print(f"  - {problem}")
        if len(problems) > 30:
            print(f"  ... mais {len(problems) - 30} pendencias")
        return 1
    print(f"EXAMPLEMOD_APK_GATE=PASS: {len(report['examples'])} exemplos integrais, "
          f"fontes, testes, Android e upstream conferidos")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
