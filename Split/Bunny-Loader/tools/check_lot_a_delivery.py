"""Gate experimental artifacts on the exact commit's independent regressions."""
import json
import sys

REQUIRED = frozenset({
    'Check Bunny CSharp Visual Queue',
    'Check CSharp native prefix gameplay v90',
    'Check CSharp combat batch',
    'Check managed callback lifecycle',
    'Check complete ExampleMod audit',
    'Check Mono pinned worker GC transitions',
    'Check Mono Android signal ownership',
    'Check Bunny CSharp Native Visual Queue',
    'Validate CSharp Native Tile Bridge',
})

CURRENT_REQUIRED = frozenset({
    'Check Bunny CSharp Visual Queue', 'Check Bunny CSharp Native Visual Queue',
    'Check CSharp combat batch', 'Check managed callback lifecycle',
    'Check complete ExampleMod audit', 'Check Mono pinned worker GC transitions',
    'Check Mono Android signal ownership', 'CSharp Native ScreenFx',
    'Check CSharp SDK and continuous runtime foundation',
})

def verdict(data, sha, required=REQUIRED):
    latest = {}
    for run in sorted(data.get('workflow_runs', []), key=lambda r: r.get('id', 0), reverse=True):
        if run.get('head_sha') == sha and run.get('event') == 'push' and run.get('name') in required:
            latest.setdefault(run['name'], run)
    for name, run in latest.items():
        if run.get('status') == 'completed' and run.get('conclusion') != 'success':
            return 1, f'Refused: {name} concluded {run.get("conclusion")}'
    missing = required - latest.keys()
    active = [name for name, run in latest.items() if run.get('status') != 'completed']
    if missing or active:
        return 2, f'Pending: missing={sorted(missing)}, active={sorted(active)}'
    return 0, f'Approved: all {len(required)} independent suites for {sha}'

if __name__ == '__main__':
    with open(sys.argv[1]) as stream:
        status, message = verdict(json.load(stream), sys.argv[2], CURRENT_REQUIRED if "--current" in sys.argv[3:] else REQUIRED)
    print(message)
    sys.exit(status)
