import json
from pathlib import Path
matrix=json.loads(Path('docs/generated/example_completion/matrix.json').read_text())
scope=json.loads(Path('tools/example_completion/lot-a-scope.json').read_text())
expected={entry['id'] for entry in matrix['examples'] if entry['plannedLot']==2}
ids=[entry['id'] for entry in scope['examples']]
assert scope['allExamplesRequired'] is True
assert scope['count']==len(ids)==len(set(ids))
assert set(ids)==expected, f'missing={expected-set(ids)} unexpected={set(ids)-expected}'
workflow=Path('../../.github/workflows/build-bunny-csharp-source-loader.yml').read_text()
assert 'run: bash Split/Bunny-Loader/tools/ci/publish_csharp_private.sh' in workflow
start=workflow.index('- name: Deliver signed APK to private Builds repository')
end=workflow.index('\n        shell:',start)
assert "steps.build_policy.outputs.publish == 'true'" in workflow[start:end]
import importlib.util
policy_spec=importlib.util.spec_from_file_location('build_policy', 'tools/ci/csharp_build_policy.py')
policy=importlib.util.module_from_spec(policy_spec)
policy_spec.loader.exec_module(policy)
repo='MantovaniDevTrue/Bunnyloader-Csharp'
assert policy.classify('push', 'refs/heads/codex/lot-a-complete-v90-29', '', '', repo)['publish']
assert not policy.classify('pull_request', 'refs/pull/1/merge', 'codex/lot-a-complete-v90-29', 'fork/repo', repo)['publish']
workflow += '\n'+Path('tools/ci/publish_csharp_private.sh').read_text()
assert 'TAG="experimental-lot-a-${GITHUB_RUN_ID}-a${GITHUB_RUN_ATTEMPT}"' in workflow
assert 'EXPERIMENTAL — Lote A1 bancada C#' in workflow
assert 'check_lot_a_delivery.py' in workflow
assert '--prerelease' in workflow
assert 'Nine regression suites required' in workflow
print(f'COMPLETE_LOT_A_SCOPE_AND_EXPERIMENTAL_DELIVERY_GUARD_PASS={len(ids)}')
