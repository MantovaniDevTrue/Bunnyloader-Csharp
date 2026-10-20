#!/usr/bin/env python3
"""Executes the selected PRs' real host tests against the combined source tree."""
import json
import os
from pathlib import Path
import subprocess
import time

root = Path(__file__).resolve().parents[2]
os.chdir(root)
output = root / 'build/ready-pr-tests'
output.mkdir(parents=True, exist_ok=True)
checks = json.loads((root / 'tools/ci/ready_pr_checks.json').read_text())
results = []
for index, check in enumerate(checks):
    started = time.monotonic()
    with (output / f'{index:02}.log').open('w') as log:
        try:
            result = subprocess.run(['bash', '-euo', 'pipefail', '-c', check['command']],
                                    stdout=log, stderr=subprocess.STDOUT, timeout=300)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
    results.append(dict(name=check['name'], source=check['source'], code=code,
                        seconds=round(time.monotonic()-started, 2)))
    print(f"{'PASS' if code == 0 else 'FAIL'} {check['name']}", flush=True)
    if code:
        print((output / f'{index:02}.log').read_text()[-8000:], flush=True)
    (output / 'results.json').write_text(json.dumps(results, indent=2)+'\n')
raise SystemExit(1 if any(row['code'] for row in results) else 0)
