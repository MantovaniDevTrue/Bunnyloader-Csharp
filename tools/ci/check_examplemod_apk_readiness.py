#!/usr/bin/env python3
"""Block APK packaging until the requested Bunny JS -> C# port is complete."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from examplemod_coverage import generate

ACCEPTED = {'validado automaticamente', 'validado no Android'}


def blockers(coverage, reference):
    rows = coverage.get('behaviors', [])
    by_id = {row['id']: row for row in rows}
    result = []
    if len(by_id) != len(rows):
        result.append('IDs de comportamento duplicados')
    # Removing pending rows from the report must not unlock the build.
    expected = {example['id'] + '#' + hook for example in reference['examples']
                for hook in example['facts']['hooksAndMethods'] or ['revisar comportamento do arquivo']}
    for key in sorted(expected - by_id.keys()):
        result.append(key + ': sem registro de tradução')
    for row in rows:
        if row.get('status') not in ACCEPTED:
            result.append(row['id'] + ': ' + str(row.get('status', 'sem estado')))
        elif not row.get('implementation') or not row.get('validatedFilesSha256'):
            result.append(row['id'] + ': implementação/evidência ausente')
        elif row['status'] == 'validado automaticamente' and not row.get('tests'):
            result.append(row['id'] + ': testes ausentes')
        elif row['status'] == 'validado no Android' and not row.get('androidEvidence'):
            result.append(row['id'] + ': evidência Android ausente')
    if not expected:
        result.append('Referência Bunny JS vazia')
    return result


def main():
    # generate checks pinned implementation/test hashes; also reject stale checked-in output.
    generated = generate()
    for path, content in generated.items():
        if path.read_text() != content:
            raise SystemExit('APK BLOQUEADO: matriz de cobertura desatualizada: ' + str(path))
    coverage = json.loads(generated[ROOT / 'docs/csharp/EXAMPLEMOD_COVERAGE.json'])
    reference = json.loads((ROOT / 'docs/csharp/reference/bunny-js-1.9.0.json').read_text())
    pending = blockers(coverage, reference)
    pending += [str(p.relative_to(ROOT)) + ': JavaScript no pacote C#'
                for p in (ROOT / 'samples/ExampleModCSharp/Playable').rglob('*.js')]
    if pending:
        print('APK BLOQUEADO: concluir a tradução e os loaders antes de empacotar.', file=sys.stderr)
        print(f'{len(pending)} pendência(s); primeiras entradas:', file=sys.stderr)
        print('\n'.join(pending[:12]), file=sys.stderr)
        return 1
    print('EXAMPLEMOD_APK_READINESS=PASS (QA Android ainda exige evidência de aparelho)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
