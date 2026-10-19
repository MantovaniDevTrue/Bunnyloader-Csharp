#!/usr/bin/env python3
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

EXPECTED_UPSTREAM = "a370a664aa3b808c87e4e4f570b27504d20df723"


def git(directory, *args):
    process = subprocess.run(
        ["git", "-C", str(directory), *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if process.returncode:
        raise RuntimeError(process.stderr.decode("utf-8", "replace").strip())
    return process.stdout


def main():
    parser = argparse.ArgumentParser(
        description="Prepara um commit na copia do Bunny original sem substituir seu historico Git."
    )
    parser.add_argument(
        "--destino", required=True, type=Path,
        help="Pasta do clone LIMPO do Bunny Loader original, em uma nova branch."
    )
    parser.add_argument(
        "--aplicar", action="store_true",
        help="Copiar os arquivos rastreados. Sem a flag, executa apenas simulacao."
    )
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    target = args.destino.resolve()

    if source == target:
        parser.error("Fonte e destino nao podem ser o mesmo diretorio.")

    for directory in (source, target):
        if not (directory / ".git").exists():
            parser.error(f"Preciso de um clone Git completo: {directory}")
        if not (directory / "app/src/main/cpp/CMakeLists.txt").is_file():
            parser.error(f"Nao e a raiz do projeto Android: {directory}")

    source_root = git(source, "rev-parse", "--show-toplevel").decode().strip()
    target_root = git(target, "rev-parse", "--show-toplevel").decode().strip()
    if Path(source_root).resolve() != source or Path(target_root).resolve() != target:
        parser.error("Ambos os caminhos devem ser a RAIZ dos respectivos clones Git.")

    upstream_commit = git(target, "rev-parse", "HEAD").decode().strip()
    if upstream_commit != EXPECTED_UPSTREAM:
        parser.error(
            f"Destino em {upstream_commit[:12]}, mas esta entrega foi comparada "
            f"com {EXPECTED_UPSTREAM[:12]}. Atualize a comparacao antes de importar."
        )
    pending = git(target, "status", "--porcelain", "--untracked-files=all")
    if pending.strip():
        parser.error("Destino possui alteracoes/untracked. Use um clone limpo e uma branch propria.")

    source_branch = git(source, "branch", "--show-current").decode().strip()
    if source_branch != "entrega/potato-bunny-original-com-csharp-20261018":
        parser.error(
            "Clone a branch entrega/potato-bunny-original-com-csharp-20261018 "
            "para obter a fonte exata desta entrega."
        )

    paths = [
        item.decode("utf-8")
        for item in git(source, "ls-files", "-z").split(b"\0")
        if item
    ]
    for relative in paths:
        file = source / relative
        if not file.is_file() or file.is_symlink():
            parser.error(f"Arquivo rastreado ausente ou symlink inesperado: {relative}")
        destination = target / relative
        if destination.is_symlink():
            parser.error(f"Destino possui symlink que requer revisao: {relative}")

    existing = sum((target / name).is_file() for name in paths)
    print(f"Fonte: {source}")
    print(f"Destino: {target}")
    print(f"Base original: {upstream_commit}")
    print(f"Arquivos rastreados para copiar: {len(paths)}")
    print(f"Destinos ja existentes (serao atualizados): {existing}")
    print(f"Novos arquivos: {len(paths) - existing}")
    print("As exclusoes nao sao realizadas. O historico .git do original nao e copiado.")

    if not args.aplicar:
        print("SIMULACAO concluida. Use --aplicar para copiar os arquivos.")
        return

    for relative in paths:
        source_file = source / relative
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_file, destination)
    print("Fonte copiada. Confira 'git diff --stat', 'git status' e os documentos")
    print("'README.md' e 'POTATO-C-SHARP-HANDOFF.md' antes de fazer commit.")
    print("Nenhum commit, push, merge, assinatura ou build Android foi executado.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError) as exc:
        print(f"ERRO: {exc}", file=sys.stderr)
        raise SystemExit(1)
