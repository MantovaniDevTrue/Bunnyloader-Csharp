import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "samples/ExampleModCSharp/Compatibility/npc_methods.json"
CLASS_RE = re.compile(r"^export class (\w+) extends (ModNPC|GlobalNPC)\b", re.MULTILINE)
METHOD_RE = re.compile(r"^    ([A-Za-z_]\w*)\([^\n]*\)\s*\{", re.MULTILINE)
ALLOWED = {"validated", "partial", "pending"}


def analyze():
    spec = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if spec.get("schemaVersion") != 1:
        raise ValueError("Unsupported NPC coverage manifest schema")
    discovered = {}
    seen_files = set()
    for source in spec["sources"]:
        relative = source["path"]
        if relative in seen_files or not relative.startswith("samples/ExampleMod/content/"):
            raise ValueError(f"Duplicate/invalid NPC reference: {relative}")
        seen_files.add(relative)
        path = ROOT / relative
        body = path.read_text(encoding="utf-8")
        classes = CLASS_RE.findall(body)
        expected_class = (source["className"], source["baseClass"])
        if classes != [expected_class]:
            raise ValueError(f"NPC class mismatch: {relative}: {classes!r}")
        actual = METHOD_RE.findall(body)
        if actual != source["methods"]:
            raise ValueError(
                f"NPC methods changed: {relative}\n"
                f"expected: {source['methods']!r}\nfound: {actual!r}"
            )
        if len(actual) != len(set(actual)):
            raise ValueError(f"Duplicate callbacks in {relative}")
        discovered[source["className"]] = actual

    names = {method for methods in discovered.values() for method in methods}
    methods = spec.get("methods", {})
    if names != set(methods):
        raise ValueError(
            f"Unclassified NPC methods: {sorted(names - set(methods))}\n"
            f"Stale classifications: {sorted(set(methods) - names)}"
        )
    if any(info["status"] not in ALLOWED or not info.get("detail") for info in methods.values()):
        raise ValueError("Every NPC callback needs a documented status and reason")

    sdk = (ROOT / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text()
    loader = (ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
    native = (ROOT / "app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()
    example = (ROOT / "samples/ExampleModCSharp/Playable/content/Content/NPCs/ExampleSlimeNPC.cs").read_text()
    guards = [
        ("SDK BunnyNPC class", "public abstract class BunnyNPC" in sdk),
        ("SDK SetDefaults API", "public abstract void SetDefaults()" in sdk),
        ("SourceLoader BunnyNPC", 'type.base == "BunnyNPC"' in loader),
        ("native NPC registration", "NativeNpc" in loader),
        ("native NPC defaults bridge", "applyManagedNativeNpcDefaultsBatch" in native),
        ("C# slime example", "class ExampleSlimeNPC : BunnyNPC" in example),
    ]
    for name, ok in guards:
        if not ok:
            raise ValueError(f"Regression detected: {name}")
    features = spec.get("slimeFeatures", [])
    if len(features) < 10 or any(row.get("status") not in ALLOWED for row in features):
        raise ValueError("Slime feature audit is missing or invalid")
    return {
        "files": len(discovered),
        "implementations": sum(map(len, discovered.values())),
        "distinctMethods": len(names),
        "statuses": {
            status: sum(info["status"] == status for info in methods.values())
            for status in ["validated", "partial", "pending"]
        },
        "sourceClasses": {name: len(methods) for name, methods in discovered.items()},
        "uncoveredSlimeFeatures": [
            row["name"] for row in features if row["status"] != "validated"
        ],
    }


def main():
    parser = argparse.ArgumentParser(description="Check NPC C# coverage against ExampleModBunny")
    parser.add_argument("--json", action="store_true", help="Print machine-readable coverage")
    args = parser.parse_args()
    try:
        result = analyze()
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f"BUNNY_NPC_COVERAGE_CHECK=FAIL: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print(
            "BUNNY_NPC_COVERAGE_CHECK=PASS "
            f"files={result['files']} methods={result['distinctMethods']} "
            f"usages={result['implementations']} "
            f"partial={result['statuses']['partial']} "
            f"pending={result['statuses']['pending']}"
        )
        print("NPC API status is documented, not assumed from successful Roslyn compilation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
