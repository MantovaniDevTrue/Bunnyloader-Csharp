"""Exercise inventory accounting, review receipts and publication decisions."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('loader_gate', ROOT/'tools/loader_batch_gate.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)
plan = json.loads(gate.PLAN.read_text())
gate.validate(plan)
assert len(gate.release_errors(plan)) == 5

def reject(mutator):
    changed = copy.deepcopy(plan)
    mutator(changed)
    try:
        gate.validate(changed)
    except ValueError:
        return
    raise AssertionError('Invalid batch inventory accepted')

reject(lambda p: p['batches'][0]['loaders'].pop())
reject(lambda p: p['batches'][0]['loaders'].append(copy.deepcopy(p['batches'][0]['loaders'][0])))
reject(lambda p: p['batches'][0]['loaders'][0]['modules'].append('FakeLoader'))
reject(lambda p: p['batches'][0]['loaders'][0]['modules'].remove('TileWorldLoader'))
reject(lambda p: p['batches'][1]['loaders'][0]['modules'].append('TileWorldLoader'))
reject(lambda p: p['batches'][0]['loaders'][0].update(contractStatus='complete'))

# Synthetic complete receipts verify the gate's positive path; they do not
# certify the real loaders. Production data above must still reject all five.
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    changed = copy.deepcopy(plan)
    for batch in plan['batches']:
        for entry in batch['loaders']:
            for module in entry['modules']:
                path = root/'app/src/main/cpp/script/js/mod/Loaders'/(module+'.js')
                path.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(ROOT/path.relative_to(root), path)
    for role in ('sdk.cs','native.cpp','runtime_test.cpp','example.cs'):
        (root/role).write_text('// synthetic gate fixture\n')
    active = changed['batches'][0]
    for entry in active['loaders']:
        path=root/entry['contractSource'];path.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(ROOT/entry['contractSource'],path)
        contract=gate.source_contract(root,entry)
        coverage={str(m['line'])+':'+m['name']:{'state':'implemented','sdk':'sdk.cs','native':'native.cpp','runtimeTest':'runtime_test.cpp'} for m in contract['members']}
        import hashlib
        receipt={'loader':entry['loader'],'sourceContract':contract,'coverage':coverage,
                 'moduleHashes':{m:hashlib.sha256((root/'app/src/main/cpp/script/js/mod/Loaders'/(m+'.js')).read_bytes()).hexdigest() for m in entry['modules']},
                 'exampleFidelityReview':'Synthetic receipt, not real implementation evidence','exampleSource':'example.cs'}
        entry.update(contractStatus='complete',exampleStatus='complete',missing=[],tests=['runtime_test.cpp'],contractReceipt=entry['loader']+'.json')
        (root/entry['contractReceipt']).write_text(json.dumps(receipt))
    assert not gate.release_errors(changed,root)
    assert len(gate.release_errors(changed,root,stable=True)) == 5
    target=root/active['loaders'][0]['contractReceipt']
    receipt=json.loads(target.read_text());missing=next(iter(receipt['coverage']))
    del receipt['coverage'][missing];target.write_text(json.dumps(receipt))
    assert 'IncompleteMemberReview' in gate.release_errors(changed,root)[0]
    receipt['coverage'][missing]={'state':'implemented','sdk':'../escape.cs','native':'native.cpp','runtimeTest':'runtime_test.cpp'}
    target.write_text(json.dumps(receipt))
    assert 'external' in gate.release_errors(changed,root)[0]
    target.write_text('{}');assert gate.release_errors(changed,root)
    # A changed native JS helper also invalidates a previously complete review.
    target=root/active['loaders'][1]['contractReceipt']
    p=root/'app/src/main/cpp/script/js/mod/Loaders/WallLoader.js';p.write_text(p.read_text()+'\n// changed\n')
    assert any('ContractSourceChangedOrWrongLoader' in e for e in gate.release_errors(changed,root))
print('FIVE_LOADER_BATCH_ACCOUNTING_RECEIPTS_SOURCE_DRIFT_HOOK_COVERAGE_EXAMPLE_STABLE_ANDROID_GATE=PASS')
