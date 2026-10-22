#!/usr/bin/env python3
"""Generate conservative coverage. Discovery supplies candidates, never validation."""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/csharp'
SAMPLE = ROOT / 'samples/ExampleModCSharp/Playable'
STATUSES = ['não iniciado', 'implementado sem validar', 'validado automaticamente',
            'validado no Android', 'falhou', 'bloqueio comprovado']

def generate():
    reference = json.loads((OUT / 'reference/bunny-js-1.9.0.json').read_text())
    official = json.loads((OUT / 'reference/tmodloader-stable.json').read_text())
    evidence = json.loads((OUT / 'examplemod-evidence.json').read_text())
    for row in evidence['behaviors'].values():
        if row['status'] in ['validado automaticamente', 'validado no Android']:
            assert row.get('validatedFilesSha256'), 'Validated behavior needs pinned implementation/test evidence'
            for name, digest in row['validatedFilesSha256'].items():
                assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, f'Stale validation evidence: {name}'
    sources = sorted(SAMPLE.rglob('*.cs'))
    symbols = collections.defaultdict(list)
    for path in sources:
        for name in re.findall(r'\bclass\s+(\w+)', path.read_text()):
            symbols[name].append(str(path.relative_to(ROOT)))
    rows = []
    for example in reference['examples']:
        candidates = sorted({p for c in example['facts']['classes'] for p in symbols[c['name']]})
        for hook in example['facts']['hooksAndMethods'] or ['revisar comportamento do arquivo']:
            key = example['id'] + '#' + hook
            row = dict(id=key, origin='Bunny JS 1.9.0', source=example['path'],
                       sourceSha256=example['sha256'], behavior=hook, candidates=candidates,
                       status='não iniciado', note='Equivalência deste comportamento ainda não auditada.',
                       implementation=[], tests=[], androidEvidence=[])
            row.update(evidence['behaviors'].get(key, {}))
            assert row['status'] in STATUSES
            if row['status'] == 'validado automaticamente':
                assert row['tests'] and row.get('validationScope') == 'host; Android pendente'
            if row['status'] == 'validado no Android':
                assert row['androidEvidence']
            rows.append(row)
    for key, value in evidence['behaviors'].items():
        if key.startswith('requirement:'):
            rows.append(dict(id=key, **value))
    counts = dict(collections.Counter(r['status'] for r in rows))
    data = dict(schemaVersion=1, baseline=evidence['baseline'], statuses=STATUSES,
                note='Candidatos por nome não comprovam equivalência. Hooks são unidades iniciais de revisão; decompor efeitos e valores durante cada lote. Inventário tML é backlog, excluído destes totais.',
                inventory=dict(cSharpFiles=len(sources), jsFiles=len(reference['examples']),
                               officialFiles=len(official['files']), officialCommit=official['commit']),
                counts=counts, behaviors=rows,
                officialBacklog=[dict(source=f['path'], gitBlob=f['sha'], status='não iniciado',
                                     note='Revisar pertinência mobile e decompor por comportamento.')
                                 for f in official['files']])
    encoded = json.dumps(data, ensure_ascii=False, indent=2) + '\n'
    lines = ['# ExampleMod CSharp — cobertura rastreável', '',
             'Gerado por `python3 tools/examplemod_coverage.py`. Fontes e evidências em `reference/` e `examplemod-evidence.json`.', '',
             '**Trabalho em andamento. Não há paridade completa nem validação Android deste lote.**', '',
             f"Base: `{evidence['baseline']['commit']}`, Bunny **1.0.11 Dust/Invasion QA**. Os 174 arquivos anexados eram idênticos à base; 98 fontes C# preservados.", '',
             'FPS, memória e tempos no aparelho continuam `null`: não foram medidos. Relatos de versões antigas não validam esta versão.', '',
             'O catálogo usa cada hook JS como unidade inicial de revisão, com hash do fonte. Os efeitos internos ainda precisam ser detalhados por lote. Coincidência de nome só sugere um candidato C#; não promove status. O inventário oficial tML inclui arquivos antigos, que exigem triagem de pertinência.', '',
             '| Estado | Comportamentos |', '|---|---:|']
    lines += [f'| {status} | {counts.get(status, 0)} |' for status in STATUSES]
    lines += ['', f"Inventários: {len(sources)} fontes C#, {len(reference['examples'])} fontes JS e {len(official['files'])} fontes oficiais tML. Esses números não medem conclusão.", '',
              'Dependência encontrada: tooltips C# fornecem um plano nativo, mas `Exports.js` instala `TooltipLoader.js` para executar o comportamento. A independência completa de JS está pendente.', '',
              'Dust e invasão: o baseline contém correções posteriores aos logs Android de falha de contrato/already-running; nenhuma nova validação em aparelho foi produzida aqui.', '',
              '| Comportamento | Estado | Observação |', '|---|---|---|']
    for row in rows:
        if row['status'] != 'não iniciado' or row['id'].startswith('requirement:'):
            lines.append('| ' + row['id'].replace('|', '\\|') + ' | ' + row['status'] + ' | ' + row.get('note', '').replace('|', '\\|') + ' |')
    lines += ['', 'Todos os hooks e o backlog oficial estão em [EXAMPLEMOD_COVERAGE.json](EXAMPLEMOD_COVERAGE.json).', '',
              'Próximos lotes: tooltip nativo independente de JS; confirmação Android Dust/invasão; paridade de NPC/bosses/projéteis; tiles/entidades/persistência; players/globals/config/UI; multiplayer e integração/unload. Nenhum desses lotes é declarado completo por inventário ou compilação.', '']
    return {OUT / 'EXAMPLEMOD_COVERAGE.json': encoded, OUT / 'EXAMPLEMOD_COVERAGE.md': '\n'.join(lines)}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for path, content in generate().items():
        if args.check:
            assert path.read_text() == content, f'Coverage stale: {path}'
        else:
            path.write_text(content)
    print('EXAMPLEMOD_COVERAGE_CONSERVATIVE_GENERATION=PASS')
