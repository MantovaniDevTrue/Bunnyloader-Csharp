#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj"
BUILD = ROOT / "build/api_discovery/negative_compile"
REPORT = ROOT / "docs/generated/csharp_api_unsupported_compile.json"
SUMMARY = ROOT / "docs/generated/csharp_api_unsupported_compile.md"
RULES = ROOT / "tools/api_discovery/priority_rules.json"
CASES = [
    {"id": "item_prefix", "candidate": "item_prefixes", "error": "CS1061",
     "source": "using BunnyLoader.Source;\npublic static class UnsupportedPrefixTooltip { public static void Test(BunnyPrefix prefix) => prefix.GetTooltipLines(); }\n"},
    {"id": "item_recipes", "candidate": "item_recipes", "error": "CS1061",
     "source": "using BunnyLoader.Source;\npublic static class MissingRecipes { public static void Test(BunnyWeapon weapon) => weapon.AddRecipes(); }\n"},
    {"id": "npc_loot", "candidate": "npc_loot", "error": "CS1061",
     "source": "using BunnyLoader.Source;\npublic static class MissingLoot { public static void Test(BunnyNPC npc) => npc.ModifyNPCLoot(); }\n"},
    {"id": "projectile_tile_collision", "candidate": "projectile_collisions", "error": "CS1061",
     "source": "using BunnyLoader.Source;\npublic static class MissingTile { public static void Test(BunnyProjectile projectile) => projectile.OnTileCollide(); }\n"},
    {"id": "player_dash", "candidate": "player_dash", "error": "CS1061",
     "source": "using BunnyLoader.Source;\npublic static class MissingDash { public static void Test(BunnyPlayer player) => player.OnStartDash(); }\n"},
]

def prepare():
    rules = json.loads(RULES.read_text(encoding="utf-8"))
    known = {r["id"] for r in rules["candidates"]}
    if not all(x["candidate"] in known for x in CASES):
        raise ValueError("Unsupported contract test has no prioritized JS evidence")
    for case in CASES:
        path = BUILD / case["id"]
        path.mkdir(parents=True, exist_ok=True)
        (path / "FailOnMissingApi.cs").write_text(case["source"], encoding="utf-8")
        relative = os.path.relpath(SDK, path).replace(os.sep, "/")
        project = (
            '<Project Sdk="Microsoft.NET.Sdk">\n'
            '  <PropertyGroup>\n'
            '    <TargetFramework>net8.0</TargetFramework>\n'
            '    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n'
            '    <LangVersion>12.0</LangVersion>\n'
            '  </PropertyGroup>\n'
            '  <ItemGroup>\n'
            '    <ProjectReference Include="' + relative + '" />\n'
            '    <Compile Include="FailOnMissingApi.cs" />\n'
            '  </ItemGroup>\n'
            '</Project>\n'
        )
        (path / "NegativeProbe.csproj").write_text(project, encoding="utf-8")

def render(report):
    lines = ["# Bunny C# — verificacao negativa de APIs ainda ausentes", "",
             "Cada caso compila intencionalmente uma chamada C# **nao suportada**.",
             "O teste so passa quando Roslyn a rejeita com o diagnostico esperado.",
             "Compilacao negativa nao comprova ausencia de recursos na API JavaScript ou no Terraria.", "",
             "Total: %d/%d rejeicoes corretas." % (report["passed"], report["total"]), "",
             "| API em pesquisa | Erro Roslyn esperado | Resultado |",
             "| --- | --- | --- |"]
    for x in report["cases"]:
        lines.append("| %s | %s | %s |" % (x["candidate"], x["error"], x["status"]))
    return "\n".join(lines) + "\n"

def build():
    prepare()
    results = []
    for case in CASES:
        path = BUILD / case["id"] / "NegativeProbe.csproj"
        run = subprocess.run(
            ["dotnet", "build", str(path), "-c", "Release", "--nologo", "-v:q",
             "-p:NuGetAudit=false"], cwd=ROOT, capture_output=True,
            text=True, timeout=150,
            env={**os.environ, "DOTNET_NOLOGO": "1", "DOTNET_CLI_TELEMETRY_OPTOUT": "1"}
        )
        logs = run.stdout + "\n" + run.stderr
        status = "pass" if run.returncode != 0 and case["error"] in logs else "fail"
        if status == "fail":
            print("NEGATIVE_CASE_FAIL " + case["id"] + "\n" + logs[-3000:], file=sys.stderr)
        results.append({"id": case["id"], "candidate": case["candidate"],
                        "error": case["error"], "status": status})
    passed = sum(c["status"] == "pass" for c in results)
    report = {"schemaVersion": 1, "mode": "Expected CSharp Roslyn compile rejection",
              "total": len(results), "passed": passed, "failed": len(results) - passed,
              "cases": results}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    SUMMARY.write_text(render(report), encoding="utf-8")
    print("BUNNY_CSHARP_NEGATIVE_CONTRACTS=" + ("PASS" if passed == len(results) else "FAIL") +
          " rejected=" + str(passed) + "/" + str(len(results)))
    return 0 if passed == len(results) else 1

def check():
    prepare()
    data = json.loads(REPORT.read_text(encoding="utf-8"))
    if data["total"] != len(CASES) or data["passed"] != len(CASES) or data["failed"] != 0:
        raise ValueError("Negative contract coverage incomplete")
    expected = [(c["id"], c["candidate"], c["error"]) for c in CASES]
    actual = [(c["id"], c["candidate"], c["error"]) for c in data["cases"]]
    if actual != expected or any(c["status"] != "pass" for c in data["cases"]):
        raise ValueError("Negative contract test drift")
    if SUMMARY.read_text(encoding="utf-8") != render(data):
        raise ValueError("Negative report drift")
    print("BUNNY_CSHARP_NEGATIVE_CHECK=PASS cases=" + str(len(CASES)))

def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--prepare", action="store_true")
    mode.add_argument("--build", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        if args.prepare:
            prepare()
            print("BUNNY_CSHARP_NEGATIVE_PREPARED=" + str(len(CASES)))
        elif args.build:
            return build()
        else:
            check()
        return 0
    except (OSError, ValueError, TypeError, KeyError, subprocess.TimeoutExpired) as e:
        print("BUNNY_CSHARP_NEGATIVE_ERROR: " + str(e), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
