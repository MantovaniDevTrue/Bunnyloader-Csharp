import importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('delivery',Path(__file__).resolve().parents[1]/'tools/check_lot_a_delivery.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
rows=[dict(id=i+1,name=n,event='push',head_sha='current',status='completed',conclusion='success') for i,n in enumerate(sorted(m.CURRENT_REQUIRED))]
assert len(m.CURRENT_REQUIRED)==9
assert m.verdict({'workflow_runs':rows},'current',m.CURRENT_REQUIRED)[0]==0
assert m.verdict({'workflow_runs':rows},'other',m.CURRENT_REQUIRED)[0]==2
for conclusion in ['failure','cancelled','timed_out']:
    assert m.verdict({'workflow_runs':rows+[dict(rows[0],id=100,conclusion=conclusion)]},'current',m.CURRENT_REQUIRED)[0]==1
assert m.verdict({'workflow_runs':rows[:-1]},'current',m.CURRENT_REQUIRED)[0]==2
assert m.verdict({'workflow_runs':[dict(r,event='pull_request') for r in rows]},'current',m.CURRENT_REQUIRED)[0]==2
print('CURRENT_NINE_GATES_EXACT_SHA_MISSING_FAILURE_CANCELLATION_EVENT_PASS')
