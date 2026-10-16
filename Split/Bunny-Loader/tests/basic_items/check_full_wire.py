#!/usr/bin/env python3
import collections
import pathlib
import sys

expected = {
    "ITEM": {20, 21, 22, 23, 24, 25, 26},
    "TILE": {13},
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
assert sum(counts.values()) >= 42, "Expected unified ExampleMod CSharp descriptors"
item_names = {parts[1] for row in rows[1:] if (parts := row.split("\t"))[0] == "ITEM" and len(parts) > 1}
assert {"ExampleItem", "ExamplePickaxe", "ExampleHamaxe"} <= item_names, (
    f"ExampleMod general items missing from V2 export: {item_names}"
)
source = (pathlib.Path(__file__).resolve().parents[2] /
          "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
start = source.index('if (fields[0] == "ITEM")')
end = source.index('} else if (fields[0] == "PLAYER"', start)
item_branch = source[start:end]
assert 'parseBasicItemWire(fields,&record,error)' in item_branch
assert 'basicItems.size() > kSourceMaxCategory' in item_branch
assert 'continue;' in item_branch.split('basicItems.push_back(std::move(record));', 1)[1], (
    "BunnyItem V2 rows must not fall through to the PROJECTILE/WEAPON/other dispatch"
)
print("BUNNY_MANAGED_ITEM_V2_DISPATCH_GUARD=PASS", sorted(item_names))
print("BUNNY_MANAGED_DEFAULTS_V2_SCHEMA_PASS", dict(sorted(counts.items())))
