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

def verdict(data, sha):
    latest = {}
    for run in sorted(data.get('workflow_runs', []), key=lambda r: r.get('id', 0), reverse=True):
        if run.get('head_sha') == sha and run.get('event') == 'push' and run.get('name') in REQUIRED:
            latest.setdefault(run['name'], run)
    for name, run in latest.items():
        if run.get('status') == 'completed' and run.get('conclusion') != 'success':
            return 1, f'Refused: {name} concluded {run.get("conclusion")}'
    missing = REQUIRED - latest.keys()
    active = [name for name, run in latest.items() if run.get('status') != 'completed']
    if missing or active:
        return 2, f'Pending: missing={sorted(missing)}, active={sorted(active)}'
    return 0, f'Approved: all {len(REQUIRED)} independent suites for {sha}'

if __name__ == '__main__':
    with open(sys.argv[1]) as stream:
        status, message = verdict(json.load(stream), sys.argv[2])
    print(message)
    sys.exit(status)
