#!/usr/bin/env python3
"""Fail closed before APK tasks until every loader in the user-approved scope is implemented."""
import argparse
import json
from pathlib import Path


def blockers(manifest, root):
    if manifest.get('targetCount') != 15:
        return ['Scope must retain exactly 15 loaders']
    rows = manifest.get('loaders', [])
    names = [row.get('loader') for row in rows]
    errors = []
    if len(rows) != 15 or len(set(names)) != 15:
        errors.append('Fifteen distinct original loaders are required')
    for row in rows:
        name = row.get('loader', '<missing>')
        if row.get('implementation') != 'implemented':
            errors.append(f'{name}: implementation={row.get("implementation", "missing")}')
        if row.get('missing'):
            errors.append(f'{name}: {len(row["missing"])} documented gaps')
        for key in ('reference', 'publicApi', 'nativeBridge', 'example'):
            paths = row.get(key, [])
            if isinstance(paths, str):
                paths = [paths]
            if not paths or any(not (root / path).is_file() for path in paths):
                errors.append(f'{name}: {key} is missing or references absent files')
        contracts = row.get('contracts', [])
        if not contracts or any(not entry.get('implemented') or not entry.get('tests') for entry in contracts):
            errors.append(f'{name}: every original contract needs implementation and executable tests')
        for contract in contracts:
            for path in contract.get('tests', []):
                if not (root / path).is_file():
                    errors.append(f'{name}: test missing: {path}')
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', default='docs/csharp-loader-completion-15.json')
    parser.add_argument('--root', default='.')
    args = parser.parse_args()
    root = Path(args.root).resolve()
    path = Path(args.manifest)
    if not path.is_absolute():
        path = root / path
    try:
        manifest = json.loads(path.read_text())
        reasons = blockers(manifest, root)
    except (OSError, ValueError, TypeError) as error:
        reasons = ['Unreadable completion manifest: ' + str(error)]
    if reasons:
        print('APK_BLOCKED_INCOMPLETE_LOADER_SCOPE')
        for reason in reasons:
            print('- ' + reason)
        raise SystemExit(1)
    print('APK_SOURCE_COMPLETENESS_GATE_PASS: 15 loaders; run required CI checks before publication')


if __name__ == '__main__':
    main()
