#!/usr/bin/env python3
"""Check maintained navigation, canonical paths and the delivered checkpoint offline.

Historical note bodies and upstream JS documentation are outside the link-check
scope; their indexes are checked. No network, game inputs or Android SDK required.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CANONICAL = "Split/Bunny-Loader"
ALIASES = {
    "android-app": "app/src/main",
    "csharp-api": "experiments/csharp/BunnyCsSdk",
    "csharp-host": "app/src/main/cpp/experiments/csharp",
    "examplemod-csharp": "samples/ExampleModCSharp/Playable/content",
    "roslyn-compiler": "experiments/csharp/BunnySourceCompiler",
}
SAMPLES = ("StarterModCSharp", "DeclaredContentCSharp", "NativeAccessoryCSharp",
           "ArmorPoliciesCSharp", "ExampleModCSharp")
LINK = re.compile(r'!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+"[^"\n]*")?\)')
FENCE = re.compile(r'^\s*(`{3,}|~{3,})')


def prose(markdown: str) -> str:
    """Ignore fenced examples so a C# snippet cannot create phantom links."""
    lines = []
    fence = None
    for line in markdown.splitlines():
        match = FENCE.match(line)
        if match:
            mark = match.group(1)
            if fence is None:
                fence = mark[0], len(mark)
            elif mark[0] == fence[0] and len(mark) >= fence[1]:
                fence = None
            lines.append("")
        else:
            lines.append(line if fence is None else "")
    return "\n".join(lines)


def anchors(markdown: str) -> set[str]:
    result: set[str] = set()
    counts: dict[str, int] = {}
    for line in prose(markdown).splitlines():
        match = re.match(r'^#{1,6}\s+(.+?)\s*#*$', line)
        if not match:
            continue
        title = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', match.group(1))
        slug = re.sub(r'[^\w\- ]', '', title.lower()).replace(' ', '-')
        number = counts.get(slug, 0)
        counts[slug] = number + 1
        result.add(slug + (f'-{number}' if number else ''))
    return result


def link_errors(path: Path, root: Path) -> list[str]:
    errors = []
    content = path.read_text(encoding="utf-8")
    for match in LINK.finditer(prose(content)):
        value = match.group(1).strip('<>')
        url = urlsplit(value)
        if url.scheme or url.netloc:
            continue
        target = (path.parent / unquote(url.path)).resolve() if url.path else path.resolve()
        if not target.is_relative_to(root.resolve()):
            errors.append(f"{path.relative_to(root)}: target escapes repository: {value}")
        elif not target.exists():
            errors.append(f"{path.relative_to(root)}: missing target: {value}")
        elif url.fragment and target.suffix == '.md':
            if unquote(url.fragment) not in anchors(target.read_text(encoding='utf-8')):
                errors.append(f"{path.relative_to(root)}: missing anchor: {value}")
    return errors


def maintained_docs(root: Path) -> list[Path]:
    paths = [root / name for name in ("README.md", "CONTRIBUTING.md", "CHANGELOG.md",
             "src/README.md", "src/javascript-bridge/README.md", "examples/README.md",
             "tests/README.md", "docs/history/README.md")]
    paths.extend((root / 'docs').glob('*.md'))
    paths.extend(root / CANONICAL / 'samples' / sample / 'README.md' for sample in SAMPLES)
    return sorted(set(paths))


def check_checkpoint(root: Path) -> list[str]:
    errors = []
    checkpoint = json.loads((root / 'docs/checkpoint.json').read_text())
    loader = checkpoint['loader']
    app = (root / CANONICAL / 'app/build.gradle.kts').read_text()
    for name in ('versionName', 'versionCode'):
        actual = re.search(rf'\b{name}\s*=\s*(?:"([^"]+)"|(\d+))', app)
        if not actual or str(loader[name]) != (actual.group(1) or actual.group(2)):
            errors.append(f'checkpoint loader.{name} differs from app/build.gradle.kts')
    if checkpoint.get('schemaVersion') != 1:
        errors.append('Unsupported checkpoint schemaVersion')
    for name in ('sourceCommit', 'apkSha256'):
        length = 40 if name == 'sourceCommit' else 64
        if not re.fullmatch(rf'[0-9a-f]{{{length}}}', loader[name]):
            errors.append(f'Invalid checkpoint loader.{name}')
    # Compare the actual deterministic package bytes, not a second handwritten version.
    spec = importlib.util.spec_from_file_location('bunny_pack', root / CANONICAL / 'tools/mods/pack.py')
    packer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packer)
    for package in checkpoint['packages']:
        sample = root / CANONICAL / 'samples' / package['sample'] / 'Playable'
        manifest = json.loads((sample / 'manifest.json').read_text())
        if manifest['version'] != package['version']:
            errors.append(f"{package['sample']}: checkpoint version differs from manifest")
        data = packer.pack_bytes(sample)
        if len(data) != package['size'] or hashlib.sha256(data).hexdigest() != package['sha256']:
            errors.append(f"{package['sample']}: package differs from delivered checkpoint; update version/evidence deliberately")
    return errors


def check(root: Path = ROOT) -> tuple[list[str], int]:
    errors = []
    docs = maintained_docs(root)
    for path in docs:
        if not path.is_file():
            errors.append(f'Missing maintained document: {path.relative_to(root)}')
        else:
            errors.extend(link_errors(path, root))
    for alias, relative in ALIASES.items():
        path = root / 'src' / alias
        expected = root / CANONICAL / relative
        if not path.is_symlink() or path.resolve() != expected.resolve() or not path.exists():
            errors.append(f'Incorrect source alias: src/{alias}')
    for path in (root / 'src/javascript-bridge').rglob('*'):
        if path.is_file() and path.suffix in {'.cpp', '.h'}:
            errors.append(f'Stale duplicate implementation: {path.relative_to(root)}')
    sdk = root / CANONICAL / 'experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj'
    projects = [sdk] + [p for sample in SAMPLES for p in
                       (root / CANONICAL / 'samples' / sample).glob('*.csproj')]
    for project in projects:
        for item in ET.parse(project).iter():
            if item.tag not in {'Compile', 'ProjectReference'} or 'Include' not in item.attrib:
                continue
            relative = item.attrib['Include'].replace('\\', '/')
            if not any(char in relative for char in '*?$(') and not (project.parent / relative).exists():
                errors.append(f'{project.relative_to(root)}: missing {relative}')
    errors.extend(check_checkpoint(root))
    return errors, len(docs)


def main() -> int:
    try:
        errors, count = check()
    except (OSError, ValueError, KeyError, TypeError, ET.ParseError) as error:
        print(f'REPOSITORY_CHECK_FAILED: {error}', file=sys.stderr)
        return 1
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'REPOSITORY_CHECK_PASS docs={count} aliases={len(ALIASES)} checkpoint=verified packages=2')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
