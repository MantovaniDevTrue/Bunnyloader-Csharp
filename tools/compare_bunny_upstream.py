#!/usr/bin/env python3
"""Produce an offline tracked-file comparison. Never modify either checkout."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PREFIX = 'Split/Bunny-Loader/'


def git(root: Path, *args: str) -> str:
    return subprocess.check_output(['git', '-C', str(root), *args]).decode()


def tracked(root: Path, prefix: str = '') -> dict[str, str]:
    result = {}
    for name in git(root, 'ls-files', '-z').split('\0'):
        if not name or not name.startswith(prefix):
            continue
        path = root / name
        if path.is_symlink():
            data = str(path.readlink()).encode()
        elif path.is_file():
            data = path.read_bytes()
        else:
            # No submodule traversal; a directory/deleted worktree entry is not a source file.
            continue
        result[name[len(prefix):]] = hashlib.sha256(data).hexdigest()
    return result


def compare(local: Path, upstream: Path) -> dict:
    current = tracked(local, PREFIX)
    reference = tracked(upstream)
    return {
        'localCommit': git(local, 'rev-parse', 'HEAD').strip(),
        'upstreamCommit': git(upstream, 'rev-parse', 'HEAD').strip(),
        'changed': sorted(k for k in current.keys() & reference.keys() if current[k] != reference[k]),
        'onlyCSharpCheckout': sorted(current.keys() - reference.keys()),
        'onlyUpstreamCheckout': sorted(reference.keys() - current.keys()),
        'unchangedCount': sum(current[k] == reference[k] for k in current.keys() & reference.keys()),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('upstream', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    upstream = args.upstream.resolve()
    # Even an explicit output must not overwrite source in either checkout.
    if output.is_relative_to(upstream) or (output.is_relative_to(ROOT) and not output.is_relative_to(ROOT / 'build')):
        parser.error('Output must be outside both source trees')
    report = compare(ROOT, upstream)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    sections = ['# Bunny upstream comparison', '',
                'Read-only comparison of tracked files. It does not replace, merge or publish source.', '',
                f"Local commit: `{report['localCommit']}`", f"Upstream commit: `{report['upstreamCommit']}`", '',
                'Prepare a reviewed PR and run CSharp regression suites before integrating.', '']
    for key in ('changed', 'onlyCSharpCheckout', 'onlyUpstreamCheckout'):
        sections += [f'## {key} ({len(report[key])})', '']
        sections += [f'- `{name}`' for name in report[key]] or ['None.']
        sections += ['']
    (output / 'README.md').write_text('\n'.join(sections))
    print(f"UPSTREAM_COMPARE_PASS changed={len(report['changed'])} onlyCSharp={len(report['onlyCSharpCheckout'])} onlyUpstream={len(report['onlyUpstreamCheckout'])}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
