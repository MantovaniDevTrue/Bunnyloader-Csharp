#!/usr/bin/env python3
import argparse
import json
import re
import sys
from collections import defaultdict, Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JS = ROOT / "samples/ExampleMod/content"
SDK = ROOT / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs"
CS = ROOT / "samples/ExampleModCSharp/Playable/content"
EVIDENCE = ROOT / "tools/api_discovery/coverage.json"
JSON_OUT = ROOT / "docs/generated/csharp_api_catalog.json"
MD_OUT = ROOT / "docs/generated/csharp_api_catalog.md"
RECIPES_OUT = ROOT / "docs/generated/csharp_api_examples.md"
STATES = ("device_verified", "ci_verified", "experimental", "not_implemented")
JS_CLASS = re.compile(r"(?m)^export\s+(?:default\s+)?class\s+(\w+)\s+extends\s+(\w+)")
JS_METHOD = re.compile(r"(?m)^ {4}([A-Za-z_]\w*)\s*\([^\n]*\)\s*\{")
SDK_CLASS = re.compile(r"(?m)^public abstract class (Bunny\w+)")
SDK_METHOD = re.compile(r"(?m)^\s*public\s+(abstract|virtual)\s+(\w+)\s+(\w+)\s*\(([^)]*)\)")
SDK_PROPERTY = re.compile(r"(?m)^\s*public\s+(\w+)\s+(\w+)\s*\{\s*get;\s*(?:protected\s+)?set;\s*\}")
CS_CLASS = re.compile(r"(?m)^\s*public\s+(?:(?:sealed|abstract|static)\s+)?class\s+(\w+)(?:\s*:\s*(\w+))?")
TICK = chr(96)

def relative(path):
    return path.relative_to(ROOT).as_posix()

def collect_js():
    entries = []
    for path in sorted(JS.rglob("*.js")):
        source = path.read_text(encoding="utf-8")
        match = JS_CLASS.search(source)
        if match is None:
            raise ValueError("Missing exported JS class: " + relative(path))
        name, base = match.groups()
        entries.append({"path": relative(path), "name": name, "base": base,
                        "methods": sorted(set(JS_METHOD.findall(source)) - {"constructor"})})
    if not entries:
        raise ValueError("ExampleModBunny sources missing")
    return entries

def collect_sdk():
    body = SDK.read_text(encoding="utf-8")
    matches = list(SDK_CLASS.finditer(body))
    if not matches:
        raise ValueError("Bunny C# SDK empty")
    out = {}
    for idx, match in enumerate(matches):
        end = matches[idx + 1].start() if idx + 1 < len(matches) else len(body)
        part = body[match.end():end]
        out[match.group(1)] = {
            "methods": [{"kind": k, "return": r, "name": n, "parameters": p.strip()}
                        for k, r, n, p in SDK_METHOD.findall(part)],
            "properties": [{"type": t, "name": n} for t, n in SDK_PROPERTY.findall(part)]}
    return out

def collect_csharp():
    classes = []
    for path in sorted(CS.rglob("*.cs")):
        source = path.read_text(encoding="utf-8")
        for name, base in CS_CLASS.findall(source):
            classes.append({"path": relative(path), "class": name, "base": base})
    if not classes:
        raise ValueError("No C# sample classes found")
    return classes

def read_features(sdk):
    spec = json.loads(EVIDENCE.read_text(encoding="utf-8"))
    if spec.get("schemaVersion") != 1 or len(spec.get("features", [])) < 16:
        raise ValueError("Evidence manifest missing features")
    ids = set()
    for feature in spec["features"]:
        key, status = feature["id"], feature["status"]
        if key in ids or status not in STATES or not feature.get("reason"):
            raise ValueError("Invalid duplicate feature / status / description: " + key)
        ids.add(key)
        probes = feature.get("probes", [])
        if (status == "not_implemented") != (len(probes) == 0):
            raise ValueError("Incorrect evidence for status: " + key)
        if status == "not_implemented" and feature.get("example"):
            raise ValueError("Unimplemented API advertised as runnable: " + key)
        if feature.get("sdkClass") and feature["sdkClass"] not in sdk:
            raise ValueError("Missing SDK C# class for " + key)
        if feature.get("example") and not (ROOT / feature["example"]).is_file():
            raise ValueError("Missing C# example " + feature["example"])
        for proof in probes:
            path = ROOT / proof["path"]
            if not path.is_file() or proof["contains"] not in path.read_text(encoding="utf-8"):
                raise ValueError("Stale source evidence: " + key + " in " + proof["path"])
    return sorted(spec["features"], key=lambda f: (STATES.index(f["status"]), f["id"]))

def build_catalog():
    js, sdk, cs = collect_js(), collect_sdk(), collect_csharp()
    features = read_features(sdk)
    grouped = defaultdict(lambda: {"files": 0, "methods": set()})
    for entry in js:
        group = grouped[entry["base"]]
        group["files"] += 1
        group["methods"].update(entry["methods"])
    groups = {base: {"files": v["files"], "methods": sorted(v["methods"])}
              for base, v in sorted(grouped.items())}
    count = Counter(f["status"] for f in features)
    return {
        "schemaVersion": 1,
        "sources": {"javascript": "samples/ExampleMod/content",
                    "sdk": relative(SDK), "examples": "samples/ExampleModCSharp/Playable/content",
                    "manualEvidence": relative(EVIDENCE)},
        "summary": {"jsFiles": len(js), "jsBaseTypes": len(groups),
                    "jsUniqueCallbacks": sum(len(x["methods"]) for x in groups.values()),
                    "sdkClasses": len(sdk),
                    "sdkMethods": sum(len(x["methods"]) for x in sdk.values()),
                    "sdkProperties": sum(len(x["properties"]) for x in sdk.values()),
                    "csharpClasses": len(cs),
                    "features": len(features),
                    "status": {state: count[state] for state in STATES}},
        "javascriptClasses": js, "javascriptGroups": groups,
        "sdkClasses": sdk, "csharpClasses": cs, "features": features}

def markdown(report):
    s = report["summary"]
    lines = ["# API C# Bunny Loader — inventario gerado", "",
             "Fontes: ExampleModBunny JS, SDK C# e ExampleModCSharp. A presenca de um callback JS nao comprova suporte C#.", "",
             "## Totais", "",
             "- JS: %s arquivos, %s bases, %s pares base/callback." % (s["jsFiles"], s["jsBaseTypes"], s["jsUniqueCallbacks"]),
             "- C#: %s classes do SDK, %s metodos, %s propriedades, %s classes de exemplo." % (
                 s["sdkClasses"], s["sdkMethods"], s["sdkProperties"], s["csharpClasses"]),
             "- Recursos classificados: %s." % s["features"], "",
             "## Cobertura", "", "| Recurso | Estado | SDK | Exemplo |",
             "| --- | --- | --- | --- |"]
    for f in report["features"]:
        lines.append("| " + f["id"] + " | " + f["status"] + " | " +
                     (f.get("sdkClass") or "—") + " | " + (f.get("example") or "—") + " |")
    lines += ["", "## Callbacks JavaScript por classe base", "",
              "A lista de JS e apenas um roteiro de ports. Ela nao implica compatibilidade binaria ou gerenciada.", "",
              "| Base JS | Arquivos | Metodos encontrados |", "| --- | ---: | --- |"]
    for base, group in report["javascriptGroups"].items():
        lines.append("| " + base + " | " + str(group["files"]) + " | " +
                     ", ".join(group["methods"]) + " |")
    lines += ["", "## API declarada no SDK C#", ""]
    for name, info in report["sdkClasses"].items():
        lines += ["### " + name, "", "Metodos: " +
                  (", ".join(m["name"] + "(" + m["parameters"] + ")" for m in info["methods"]) or "nenhum"),
                  "", "Propriedades: " +
                  (", ".join(p["name"] for p in info["properties"]) or "nenhuma"), ""]
    lines += ["## Politica dos estados", "",
              "- device_verified: somente confirmacao explicita em dispositivo; o script nunca atribui isso sozinho.",
              "- ci_verified: fonte e contrato comprovados, nao desempenho Android.",
              "- experimental: superficie encontrada, comportamento parcial ou ainda nao comprovado.",
              "- not_implemented: ausencia do contrato C#, sem exemplo executavel.",
              "", "Gerar: python3 tools/csharp_api_discovery.py --write",
              "Conferir no CI: python3 tools/csharp_api_discovery.py --check", ""]
    return "\n".join(lines)

def recipes(report):
    lines = ["# Receitas C# extraidas do ExampleModCSharp", "",
             "Cada trecho e o arquivo C# existente completo, sem inventar API. Nao e uma traducao automatica de callbacks JS.", ""]
    seen = set()
    for feature in report["features"]:
        path = feature.get("example")
        if not path or path in seen:
            continue
        seen.add(path)
        code = (ROOT / path).read_text(encoding="utf-8").rstrip()
        lines += ["## " + feature["id"] + " — " + feature["status"], "",
                  "Fonte: " + path, "", feature["reason"], "", TICK * 3 + "csharp", code, TICK * 3, ""]
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--summary", action="store_true")
    args = parser.parse_args()
    try:
        report = build_catalog()
        output = {JSON_OUT: json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n",
                  MD_OUT: markdown(report), RECIPES_OUT: recipes(report)}
        if args.write:
            for path, body in output.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(body, encoding="utf-8")
        if args.check:
            for path, body in output.items():
                if not path.exists() or path.read_text(encoding="utf-8") != body:
                    raise ValueError("Outdated catalog: " + relative(path))
        print(json.dumps(report["summary"], ensure_ascii=False, indent=2))
        return 0
    except (ValueError, OSError, KeyError, TypeError) as e:
        print("BUNNY_CSHARP_API_DISCOVERY=FAIL: " + str(e), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
