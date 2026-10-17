#!/usr/bin/env python3
from pathlib import Path
from tempfile import TemporaryDirectory
import os
import re
import shutil
import sys
import zipfile

SOURCE_COMMIT = "dc6243850c1d7ac83eb528bc2d56cf585066fc85"
VERSION = "0.1.10.16.90.29.87.1"
ARCHIVE = f"BunnyLoader-CSharp-{VERSION}-Fontes-Organizadas.zip"
BASE = Path(__file__).resolve().parents[1]
CORE = BASE / "Split" / "Bunny-Loader"

README = """# BunnyLoader C# — Fontes organizadas (.87.1)

Snapshot de fontes do BunnyLoader C# v0.1.10.16.90.29.87.1-csharp-lights-scene.
Commit original: dc6243850c1d7ac83eb528bc2d56cf585066fc85
Versão Android: 249.

## Onde está o código?
- app/: launcher Android, integração JNI, IL2CPP, MonoVM, hooks, filas e gráficos.
- experiments/csharp/BunnyCsSdk/: classes e contratos C#.
- experiments/csharp/BunnySourceCompiler/: compilador Roslyn.
- experiments/csharp/BunnySourceSdk/: definição do projeto de compilação do SDK.
- samples/: ExampleMod C#, probes, ScreenFx e Lights And Shadows.
- tools/ e tests/: ferramentas e testes do loader.
- ProjectExtras/: documentação, espelhos e índices que estavam em outras pastas.
- .github/: workflows originais de CI e construção.

Todas as fontes versionadas do projeto foram agrupadas neste ZIP, sem misturar
código de outras versões experimentais. Arquivos textuais foram ajustados apenas
para referências de organização; nenhuma implementação de hooks/shaders foi removida.
Esta é uma edição de distribuição, não o commit compilado e validado no Android.

## Dependências que não são código-fonte deste projeto
Ver DEPENDENCIAS_EXTERNAS.md. O ZIP não inclui conteúdos proprietários do jogo,
bibliotecas binárias do runtime, chaves de assinatura ou caches de compilação.
A presença do código não significa que o projeto compile sem preparar tais entradas.
"""

DEPS = """# Dependências externas da .87.1

Estes componentes não fazem parte dos arquivos versionados do BunnyLoader e não
podem ser tratados como código-fonte próprio:

- Runtime do Terraria 1.4.5.8.6: arquivos binários/metadata/assets necessários
  ao jogo, armazenados como insumo privado, não distribuídos neste ZIP.
- Unity 2021.3.56f2: unity-classes.jar, obtido da Unity ou do cache privado.
- MonoVM ARM64, bibliotecas managed (.dll) e módulos AOT: obtidos ou produzidos
  pelo pipeline de build. Os códigos de integração e de registro AOT estão inclusos.
- QuickJS NG v0.17.0: código de terceiro normalmente baixado durante o build;
  consulte app/src/main/cpp/third_party/README.md.
- Dependências NuGet, Maven/Gradle, NDK e ferramentas de compilação externas.
- Credenciais/keystore de assinatura: intencionalmente excluídos.

Repositório de insumos de build configurado nos workflows:
https://github.com/MantovaniDevTrue/Bunnyloader-Csharp-Builds
Acesso depende das permissões do proprietário.

Para preservar a fidelidade do APK já testado, use o commit original indicado
no README, as versões externas e os scripts de build correspondentes. Este
pacote reorganiza os arquivos e não certifica um novo build Android.
"""

TEXT_SUFFIXES = {".md", ".txt", ".yml", ".yaml", ".json", ".xml", ".gradle", ".kts",
                 ".sh", ".py", ".csproj", ".props", ".targets", ".kt", ".java",
                 ".cpp", ".h", ".hpp", ".cc", ".c", ".cs", ".js", ".mjs",
                 ".html", ".css", ".ini", ".toml", ".properties", ".bat"}
PROJECT_DOC_SUFFIXES = {".md", ".txt", ".rst"}

def fix_text(path):
    if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in {"gradlew", "Makefile", ".gitignore"}:
        return
    try:
        raw = path.read_bytes()
        if b"\0" in raw:
            return
        data = raw.decode("utf-8")
    except (UnicodeError, OSError):
        return
    orig = data
    data = data.replace("Split/Bunny-Loader", ".").replace("Split\\Bunny-Loader", ".")
    if path.suffix.lower() in PROJECT_DOC_SUFFIXES:
        data = re.sub(r"\bTL[- ]?Pro\b", "BunnyLoader", data, flags=re.IGNORECASE)
        data = re.sub(r"(?<![.\w])Split\b(?!\s*\()", "BunnyLoader", data)
    if data != orig:
        path.write_text(data, encoding="utf-8")

def copy_entry(src, dest):
    if not src.exists() and not src.is_symlink():
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    if src.is_symlink():
        target = src.resolve()
        if target.is_file():
            shutil.copy2(target, dest)
        elif target.is_dir():
            shutil.copytree(target, dest, dirs_exist_ok=True)
        return
    if src.is_dir():
        shutil.copytree(src, dest, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns(".git", "__pycache__", ".gradle", ".cxx", "build"))
    else:
        shutil.copy2(src, dest)

def main():
    if not CORE.is_dir():
        raise SystemExit("Diretório do fonte .87.1 não encontrado")
    output = BASE / "dist" / ARCHIVE
    output.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="bunnyloader-source-") as tmp:
        dest = Path(tmp) / "BunnyLoader"
        copy_entry(CORE, dest)
        for name in [".github", "docs", "src", "examples", "tests"]:
            copy_entry(BASE / name, dest / "ProjectExtras" / name)
        for name in ["README.md", "CHANGELOG.md", "CONTRIBUTING.md"]:
            copy_entry(BASE / name, dest / "ProjectExtras" / name)
        for path in dest.rglob("*"):
            if path.is_file() and not path.is_symlink():
                fix_text(path)
        (dest / "README.md").write_text(README, encoding="utf-8")
        (dest / "DEPENDENCIAS_EXTERNAS.md").write_text(DEPS, encoding="utf-8")
        files = sorted(p for p in dest.rglob("*") if p.is_file())
        cs = sum(p.suffix == ".cs" for p in files)
        cpp = sum(p.suffix in {".cpp", ".h"} for p in files)
        checks = [
            "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp",
            "app/src/main/cpp/graphics/CSharpScreenGraph.cpp",
            "experiments/csharp/BunnyCsSdk/BunnyUniversalHooks.cs",
            "experiments/csharp/BunnySourceCompiler/Compiler.cs",
            "samples/ExampleModCSharp/Playable/content/main.cs",
            "samples/LightsAndShadowsCSharp/Playable",
        ]
        absent = [item for item in checks if not (dest / item).exists()]
        if absent:
            raise SystemExit("Fontes esperados ausentes: " + ", ".join(absent))
        if len(files) < 2800 or cs < 160 or cpp < 350:
            raise SystemExit(f"Arquivos insuficientes: total={len(files)} CS={cs} C++={cpp}")
        badpaths = [str(p.relative_to(dest)) for p in files if "Split" in str(p.relative_to(dest)) or "TLPro" in str(p.relative_to(dest))]
        if badpaths:
            raise SystemExit("Pastas antigas no ZIP: " + ", ".join(badpaths[:12]))
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for file in files:
                z.write(file, file.relative_to(dest.parent))
        with zipfile.ZipFile(output) as z:
            problem = z.testzip()
            if problem:
                raise SystemExit("ZIP inválido: " + problem)
        print(f"ZIP={output}")
        print(f"FILES={len(files)} CSHARP={cs} CPP_HEADERS={cpp} BYTES={output.stat().st_size}")

if __name__ == "__main__":
    main()
