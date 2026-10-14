#!/usr/bin/env python3
import collections
import pathlib
import sys

expected = {
    "ITEM": {20},
    "PROJECTILE": {8},
    "WEAPON": {6},
    "BUFF": {3},
    "POTION": {4},
    "ACCESSORY": {3, 4, 9},
    "PLAYER": {4, 5, 8, 9, 14},
    "NPC": {10},
    "PREFIX": {16},
}
rows = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
assert rows and rows[0] == "BUNNY_MANAGED_DEFAULTS_V2", "V2 header missing"
counts = collections.Counter()
errors = []
for line_number, row in enumerate(rows[1:], start=2):
    if not row:
        continue
    fields = row.split("\t")
    kind = fields[0]
    counts[kind] += 1
    if len(fields) not in expected.get(kind, set()):
        errors.append(f"line={line_number} kind={kind!r} class={fields[1] if len(fields)>1 else '<missing>'!r} fields={len(fields)} expected={sorted(expected.get(kind, []))}")
    elif len(fields) > 1 and not fields[1]:
        errors.append(f"line={line_number} kind={kind!r} missing class")
if errors:
    raise SystemExit("BUNNY_MANAGED_DEFAULTS_V2_SCHEMA_FAIL\n" + "\n".join(errors))
assert sum(counts.values()) >= 31, "Expected all ExampleMod CSharp descriptors"
print("BUNNY_MANAGED_DEFAULTS_V2_SCHEMA_PASS", dict(sorted(counts.items())))
