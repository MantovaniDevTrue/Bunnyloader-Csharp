#!/usr/bin/env python3
"""Account for every original module; release only reviewed batches of five systems.

An audit receipt records a review, not a gameplay certificate. Existing same-SHA
CI/Android build/signature gates still apply after this gate. Android validation
is separately required for a stable release.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT.parents[1] / 'docs/csharp-loader-batches.json'
MEMBER = re.compile(r'^    (?:(?:static|get|set) )*(?!#)([A-Za-z_]\w*)\s*(?:\(([^\n]*?)\)\s*\{|=)', re.M)


def source_contract(root, entry):
    source = root / entry['contractSource']
    text = source.read_text()
    # These are source anchors for manual review, not an automatic API model.
    return {'source': entry['contractSource'],
            'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'members': [{'name': m[1], 'line': text.count('\n', 0, m.start()) + 1}
                        for m in MEMBER.finditer(text)]}


def validate(plan, root=ROOT):
    if plan.get('schemaVersion') != 1 or plan['policy']['loadersPerApk'] != 5 or plan['policy']['completeExamplesPerLoader'] != 1:
        raise ValueError('Expected five loaders and one complete test example per loader')
    names, modules, batches = [], [], []
    for batch in plan['batches']:
        batches.append(batch['id'])
        if len(batch['loaders']) != 5:
            raise ValueError('Batch must contain exactly five systems: ' + batch['id'])
        for entry in batch['loaders']:
            names.append(entry['loader'])
            modules.extend(entry['modules'])
            if entry['contractStatus'] not in ('pending', 'partial', 'complete'):
                raise ValueError('Unknown contract state: ' + entry['loader'])
            if entry['exampleStatus'] not in ('pending', 'partial', 'implemented-awaiting-android', 'complete'):
                raise ValueError('Unknown example state: ' + entry['loader'])
            if entry['contractStatus'] == 'complete' and entry['missing']:
                raise ValueError('Complete loader still has missing functionality: ' + entry['loader'])
    for what, values in [('loader', names), ('module', modules), ('batch', batches)]:
        duplicates = [name for name, count in Counter(values).items() if count != 1]
        if duplicates:
            raise ValueError('Duplicate ' + what + ': ' + ','.join(duplicates))
    actual = {p.stem for p in (root / 'app/src/main/cpp/script/js/mod/Loaders').glob('*.js')}
    if set(modules) != actual:
        raise ValueError('Original module inventory mismatch Missing=' + ','.join(sorted(actual-set(modules))) +
                         ' Unexpected=' + ','.join(sorted(set(modules)-actual)))
    if plan['activeBatch'] not in batches:
        raise ValueError('Active batch not found')


def local_file(root, value):
    if not isinstance(value, str) or not value or Path(value).is_absolute():
        raise ValueError('Expected relative source/evidence path')
    path = (root / value).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError('Missing or external source/evidence: ' + value)
    return path


def release_errors(plan, root=ROOT, stable=False):
    validate(plan, root)
    active = next(b for b in plan['batches'] if b['id'] == plan['activeBatch'])
    errors = []
    for entry in active['loaders']:
        name = entry['loader']
        if entry['contractStatus'] != 'complete' or entry['exampleStatus'] != 'complete' or entry['missing']:
            errors.append(name + ': ContractOrExampleIncomplete')
            continue
        try:
            receipt = json.loads(local_file(root, entry.get('contractReceipt')).read_text())
            contract = source_contract(root, entry)
            module_hashes = {m: hashlib.sha256((root / 'app/src/main/cpp/script/js/mod/Loaders' / (m+'.js')).read_bytes()).hexdigest()
                             for m in entry['modules']}
            if receipt['loader'] != name or receipt['sourceContract'] != contract or receipt['moduleHashes'] != module_hashes:
                raise ValueError('ContractSourceChangedOrWrongLoader')
            expected = {str(m['line']) + ':' + m['name'] for m in contract['members']}
            coverage = receipt['coverage']
            if not expected or set(coverage) != expected:
                raise ValueError('IncompleteMemberReview')
            for feature in coverage.values():
                if feature['state'] != 'implemented':
                    raise ValueError('UnimplementedContractMember')
                for key in ('sdk', 'native', 'runtimeTest'):
                    local_file(root, feature[key])
            if not entry['example'] or not receipt['exampleFidelityReview'].strip():
                raise ValueError('MissingOriginalExampleReview')
            local_file(root, receipt['exampleSource'])
            if not entry['tests']:
                raise ValueError('MissingExecutableTests')
            for test in entry['tests']:
                local_file(root, test)
            if stable:
                if entry['android'] != 'validated':
                    raise ValueError('AndroidGameplayPending')
                local_file(root, entry.get('androidEvidence'))
        except (ValueError, KeyError, TypeError) as error:
            errors.append(name + ': ' + str(error))
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--delivery', action='store_true')
    parser.add_argument('--stable', action='store_true')
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    plan = json.loads(PLAN.read_text())
    validate(plan)
    if args.delivery or args.stable:
        errors = release_errors(plan, stable=args.stable)
        if errors:
            print('PublicationHeldFiveLoaderBatch: ' + ' | '.join(errors))
            raise SystemExit(1)
        print('FIVE_LOADER_REVIEW_GATE=PASS; same-SHA CI/build/signature gates still required; gameplay separate')
        return
    active = next(b for b in plan['batches'] if b['id'] == plan['activeBatch'])
    contracts = {'schemaVersion': 1, 'batch': active['id'],
                 'meaning': 'Source anchors for manual review. Presence or compilation is not implementation evidence.',
                 'contracts': {e['loader']: source_contract(ROOT, e) for e in active['loaders']}}
    target = ROOT.parents[1] / 'docs/generated/csharp-world-loader-contracts.json'
    body = json.dumps(contracts, ensure_ascii=False, indent=2) + '\n'
    if args.write:
        target.write_text(body)
    elif not target.exists() or target.read_text() != body:
        raise SystemExit('Stale world loader source contracts: run --write')
    print('LOADER_BATCH_INVENTORY=PASS systems=' + str(sum(len(b['loaders']) for b in plan['batches'])) + ' originalModules=' + str(sum(len(e['modules']) for b in plan['batches'] for e in b['loaders'])) + ' active=' + active['id'] + '; delivery readiness evaluated separately')


if __name__ == '__main__':
    main()
