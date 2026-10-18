#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "docs/generated/csharp_api_catalog.json"
RULES = ROOT / "tools/api_discovery/priority_rules.json"
OUT_JSON = ROOT / "docs/generated/csharp_api_missing_priorities.json"
OUT_MD = ROOT / "docs/generated/csharp_api_missing_priorities.md"
LEVELS = {1, 2, 3}

def produce():
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    spec = json.loads(RULES.read_text(encoding="utf-8"))
    if spec.get("schemaVersion") != 1:
        raise ValueError("Unsupported priority rules schema")
    classes = catalog["javascriptClasses"]
    groups = catalog["javascriptGroups"]
    features = {f["id"]: f for f in catalog["features"]}
    candidates = []
    unique = set()
    for item in spec["candidates"]:
        key = item["id"]
        if key in unique or item.get("priority") not in LEVELS or not item.get("reason"):
            raise ValueError("Invalid candidate: " + key)
        unique.add(key)
        base = item["jsBase"]
        if base not in groups:
            raise ValueError("Missing JS ExampleModBunny base: " + base)
        wanted = item["callbacks"]
        if not wanted or len(wanted) != len(set(wanted)):
            raise ValueError("Bad JS callbacks in " + key)
        for callback in wanted:
            if callback not in groups[base]["methods"]:
                raise ValueError("Missing ExampleModBunny JS callback: " + base + "." + callback)
        references = []
        for entry in classes:
            if entry["base"] != base:
                continue
            relevant = sorted(set(entry["methods"]) & set(wanted))
            if relevant:
                references.append({"file": entry["path"], "class": entry["name"], "callbacks": relevant})
        if not references:
            raise ValueError("No JS source for " + key)
        candidates.append({
            "id": key, "priority": item["priority"], "jsBase": base, "callbacks": wanted,
            "target": item["target"], "reason": item["reason"], "referenceCount": len(references),
            "jsSourceExamples": sorted(references, key=lambda x: x["file"])[:4],
            "state": "research_only", "implementedInCSharp": False
        })
    candidates.sort(key=lambda x: (x["priority"], -x["referenceCount"], x["id"]))
    return {"schemaVersion": 1,
            "warning": "JS references are suggestions, not supported CSharp signatures. No tModLoader API has been copied.",
            "baselineFeatures": len(features), "candidates": candidates,
            "summary": {"total": len(candidates), "priority1": sum(x["priority"] == 1 for x in candidates),
                        "priority2": sum(x["priority"] == 2 for x in candidates),
                        "priority3": sum(x["priority"] == 3 for x in candidates)}}

def markdown(report):
    lines = ["# Bunny Loader C# — lacunas de API priorizadas", "",
             "**Somente pesquisa:** assinaturas JS do ExampleModBunny nao sao APIs C# prontas.",
             "Esta lista indica o que implementar e testar futuramente; nenhuma entrada e promocao automatica de suporte.", "",
             "## Candidatos", "", "| Prioridade | Recurso | JS base e callbacks | Fontes JS | Alvo C# proposto |",
             "| --- | --- | --- | ---: | --- |"]
    for x in report["candidates"]:
        lines.append("| P%s | %s | %s: %s | %s | %s |" % (
            x["priority"], x["id"], x["jsBase"], ", ".join(x["callbacks"]),
            x["referenceCount"], x["target"]))
    lines += ["", "## Arquivos-fonte JS que fundamentam cada candidato", ""]
    for x in report["candidates"]:
        lines += ["### " + x["id"], "", x["reason"], "",
                  "Nenhuma assinatura C# e inferida desta lista. Exemplos JavaScript para estudo:"]
        for entry in x["jsSourceExamples"]:
            lines.append("- " + entry["file"] + " (" + entry["class"] +
                         ": " + ", ".join(entry["callbacks"]) + ")")
        lines += [""]
    lines += ["## Como promover uma API", "",
              "Implementar primeiro o contrato real C# no SDK e na ponte Bunny; adicionar um pequeno ExampleModCSharp.",
              "Compilar com Roslyn, verificar em CI e confirmar no Android para o status device_verified.",
              "A versao .86 validada nao e modificada por estas ferramentas.", ""]
    return "\n".join(lines)

def main():
    p = argparse.ArgumentParser()
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    group.add_argument("--summary", action="store_true")
    args = p.parse_args()
    try:
        report = produce()
        paths = {OUT_JSON: json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n",
                 OUT_MD: markdown(report)}
        if args.write:
            for path, value in paths.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(value, encoding="utf-8")
        if args.check:
            for path, value in paths.items():
                if not path.is_file() or path.read_text(encoding="utf-8") != value:
                    raise ValueError("Outdated API gap report: " + str(path))
        print(json.dumps(report["summary"], ensure_ascii=False))
        return 0
    except (ValueError, OSError, TypeError, KeyError) as error:
        print("BUNNY_API_GAPS=FAIL: " + str(error), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
