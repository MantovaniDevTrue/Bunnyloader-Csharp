#!/usr/bin/env python3
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "samples/ExampleModCSharp/Playable/content"
SDK = ROOT / "experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj"
CATALOG = ROOT / "docs/generated/csharp_api_catalog.json"
BUILD = ROOT / "build/api_discovery/compile"
REPORT = ROOT / "docs/generated/csharp_api_compile_results.json"
SUMMARY = ROOT / "docs/generated/csharp_api_compile_results.md"
CASES = {
    "weapon_wand": ("weapon_registration", ["Content/Items/Weapons/ExampleWand.cs", "main.cs"]),
    "weapon_wave": ("weapon_registration", ["Content/Items/Weapons/ExampleWaveWand.cs", "Content/Projectiles/ExampleWaveBolt.cs"]),
    "projectile_bolt": ("projectile_registration", ["main.cs"]),
    "projectile_wave": ("projectile_registration", ["Content/Projectiles/ExampleWaveBolt.cs"]),
    "projectile_hit_policy": ("legacy_live_hits", ["main.cs"]),
    "buff": ("buff_definition", ["Content/Buffs/ExampleDefenseBuff.cs"]),
    "potion": ("buff_potion", ["Content/Items/Consumables/ExampleBuffPotion.cs", "Content/Buffs/ExampleDefenseBuff.cs"]),
    "accessory": ("accessory", ["Content/Items/Accessories/ExampleFocusAccessory.cs"]),
    "player_equips": ("player_equips", ["Common/Players/ExampleFocusPlayer.cs", "Content/Items/Accessories/ExampleFocusAccessory.cs"]),
    "npc_slime": ("npc_registration", ["Content/NPCs/ExampleSlimeNPC.cs"]),
    "live_legacy": ("legacy_live_hits", ["Common/Live/BunnyLiveEvents.cs"]),
    "live_v4": ("on_hit_context_v4", ["Common/Live/ExampleWaveBoltLiveEvents.cs"]),
    "live_tick": ("legacy_live_hits", ["Common/Live/ExampleLiveRuntime.cs"]),
    "npc_ai": ("npc_ai_prototype", ["Common/Live/ExampleLiveNpcAI.cs"]),
    "motion_bolt": ("projectile_motion", ["Content/Projectiles/ExampleBoltMotion.cs"]),
    "motion_wave": ("projectile_motion", ["Content/Projectiles/ExampleWaveBoltMotion.cs"]),
}

def get_catalog():
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    status = {f["id"]: f["status"] for f in catalog["features"]}
    for name, (feature, paths) in CASES.items():
        if feature not in status or status[feature] == "not_implemented":
            raise ValueError("Unsupported compile feature: " + name)
        for relative in paths:
            p = (CONTENT / relative).resolve()
            if not p.is_relative_to(CONTENT.resolve()) or p.suffix != ".cs" or not p.is_file():
                raise ValueError("Missing or invalid Bunny C# source: " + relative)
    return catalog

def sdk_contract_source(catalog):
    src = ["using BunnyLoader.Source;", "namespace BunnyLoader.Source.ApiDiscovery;", ""]
    count_methods = 0
    count_properties = 0
    for name, spec in sorted(catalog["sdkClasses"].items()):
        src.append("public sealed class Probe" + name + " : " + name + " {")
        setters = ["this." + x["name"] + " = default!;" for x in spec["properties"]]
        count_properties += len(setters)
        for method in spec["methods"]:
            sig = "    public override " + method["return"] + " " + method["name"] + "(" + method["parameters"] + ")"
            body = "{ " + " ".join(setters) + " }" if method["name"] == "SetDefaults" else "=> throw new System.NotImplementedException();"
            src.append(sig + " " + body)
            count_methods += 1
        src.append("}")
    if count_methods < 10 or count_properties < 30:
        raise ValueError("C# SDK unexpectedly lost methods or properties")
    return "\n".join(src) + "\n", count_methods, count_properties

def create_project(name, paths):
    folder = BUILD / name
    folder.mkdir(parents=True, exist_ok=True)
    project = folder / "Probe.csproj"
    lines = ['<Project Sdk="Microsoft.NET.Sdk">', "  <PropertyGroup>",
             "    <TargetFramework>net8.0</TargetFramework>",
             "    <LangVersion>12.0</LangVersion>",
             "    <Nullable>enable</Nullable>",
             "    <ImplicitUsings>enable</ImplicitUsings>",
             "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>",
             "    <AssemblyName>Probe_" + name + "</AssemblyName>",
             "  </PropertyGroup>", "  <ItemGroup>",
             '    <ProjectReference Include="' + os.path.relpath(SDK, folder).replace(os.sep, "/") + '" />']
    for path in paths:
        lines.append('    <Compile Include="' + os.path.relpath(path, folder).replace(os.sep, "/") + '" />')
    lines += ["  </ItemGroup>", "</Project>", ""]
    project.write_text("\n".join(lines), encoding="utf-8")
    return project

def prepare():
    catalog = get_catalog()
    entries = []
    for name, (feature, paths) in CASES.items():
        create_project(name, [CONTENT / x for x in paths])
        entries.append({"id": name, "feature": feature, "source": paths})
    source, methods, properties = sdk_contract_source(catalog)
    folder = BUILD / "sdk_contracts"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / "SdkContracts.cs"
    path.write_text(source, encoding="utf-8")
    create_project("sdk_contracts", [path])
    entries.append({"id": "sdk_contracts", "feature": "sdk_all_public_members",
                    "source": ["experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs"]})
    return entries, methods, properties

def as_markdown(result):
    lines = ["# Bunny C# — compilacao de exemplos em lote", "",
             "Resultado de dotnet build com Roslyn e net8.0. Isto nao prova a execucao no Terraria Android.", "",
             "Casos aprovados: " + str(result["passed"]) + "/" + str(result["total"]),
             "SDK: " + str(result["sdkMethods"]) + " metodos, " + str(result["sdkProperties"]) + " propriedades.", "",
             "| Exemplo | Area | Compilacao | Fonte |", "| --- | --- | --- | --- |"]
    for case in result["cases"]:
        lines.append("| " + case["id"] + " | " + case["feature"] + " | " + case["result"] +
                     " | " + ", ".join(case["source"]) + " |")
    return "\n".join(lines) + "\n"

def compile_all():
    entries, methods, properties = prepare()
    results = []
    for item in entries:
        cmd = ["dotnet", "build", str(BUILD / item["id"] / "Probe.csproj"),
               "-c", "Release", "--nologo", "-v:q", "-p:NuGetAudit=false"]
        run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=150,
                             env={**os.environ, "DOTNET_NOLOGO": "1", "DOTNET_CLI_TELEMETRY_OPTOUT": "1"})
        passed = run.returncode == 0
        if not passed:
            print("CASE_FAIL " + item["id"] + "\n" + (run.stdout + "\n" + run.stderr)[-5000:], file=sys.stderr)
        results.append({**item, "result": "pass" if passed else "fail"})
    passed = sum(x["result"] == "pass" for x in results)
    report = {"schemaVersion": 1, "mode": "CI net8.0 Roslyn compile only",
              "total": len(results), "passed": passed, "failed": len(results) - passed,
              "sdkMethods": methods, "sdkProperties": properties, "cases": results}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    SUMMARY.write_text(as_markdown(report), encoding="utf-8")
    print("BUNNY_CSHARP_BATCH_COMPILE=" + ("PASS" if passed == len(results) else "FAIL") +
          " compiled=" + str(passed) + "/" + str(len(results)))
    return 0 if passed == len(results) else 1

def check():
    entries, methods, properties = prepare()
    data = json.loads(REPORT.read_text(encoding="utf-8"))
    if data["total"] != len(entries) or data["passed"] != len(entries) or data["failed"]:
        raise ValueError("Incomplete C# compile coverage")
    if data["sdkMethods"] != methods or data["sdkProperties"] != properties:
        raise ValueError("C# SDK signature drift")
    if [(x["id"], x["feature"]) for x in data["cases"]] != [(x["id"], x["feature"]) for x in entries]:
        raise ValueError("Compilation case manifest drift")
    for entry in entries:
        dll = BUILD / entry["id"] / "bin/Release/net8.0" / ("Probe_" + entry["id"] + ".dll")
        if not dll.is_file() or dll.stat().st_size == 0:
            raise ValueError("Missing compiled C# DLL " + entry["id"])
    if SUMMARY.read_text(encoding="utf-8") != as_markdown(data):
        raise ValueError("Compilation report mismatch")
    print("BUNNY_CSHARP_BATCH_CHECK=PASS cases=" + str(len(entries)))

def main():
    p = argparse.ArgumentParser()
    modes = p.add_mutually_exclusive_group(required=True)
    modes.add_argument("--prepare", action="store_true")
    modes.add_argument("--build", action="store_true")
    modes.add_argument("--check", action="store_true")
    args = p.parse_args()
    try:
        if args.prepare:
            entries, methods, props = prepare()
            print("BUNNY_CSHARP_BATCH_PREPARED=%s SDK=%s/%s" % (len(entries), methods, props))
            return 0
        if args.build:
            return compile_all()
        check()
        return 0
    except (OSError, KeyError, ValueError, TypeError, subprocess.TimeoutExpired) as error:
        print("BUNNY_CSHARP_BATCH_ERROR: " + str(error), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
