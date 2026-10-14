import importlib.util
from pathlib import Path
spec = importlib.util.spec_from_file_location('gate', Path('tools/check_lot_a_delivery.py'))
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)
runs = [{'id': i, 'name': name, 'head_sha': 'tested', 'event': 'push', 'status': 'completed', 'conclusion': 'success'}
        for i, name in enumerate(sorted(gate.REQUIRED), 1)]
assert gate.verdict({'workflow_runs': runs}, 'tested')[0] == 0
assert gate.verdict({'workflow_runs': runs}, 'other')[0] == 2
assert gate.verdict({'workflow_runs': runs[:-1]}, 'tested')[0] == 2
bad = dict(runs[0], id=20, conclusion='failure')
assert gate.verdict({'workflow_runs': runs + [bad]}, 'tested')[0] == 1
bad['conclusion'] = 'cancelled'
assert gate.verdict({'workflow_runs': runs + [bad]}, 'tested')[0] == 1
bad['status'] = 'in_progress'
assert gate.verdict({'workflow_runs': runs + [bad]}, 'tested')[0] == 2
bad['event'] = 'pull_request'
assert gate.verdict({'workflow_runs': runs + [bad]}, 'tested')[0] == 0
print('EXPERIMENTAL_DELIVERY_EXACT_COMMIT_EIGHT_SUITES_PASS')
