#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = Path(__file__).with_name("extended_hooks.json")
CS_OUT = ROOT / "experiments/csharp/BunnyCsSdk/BunnyExtendedHookModes.g.cs"
CPP_OUT = ROOT / "app/src/main/cpp/experiments/csharp/UniversalHookModes.generated.h"
SDK = ROOT / "experiments/csharp/BunnyCsSdk/BunnyUniversalHooks.cs"
CPP = ROOT / "app/src/main/cpp/experiments/csharp/UniversalHooks.cpp"


def validate(data: dict) -> list[dict]:
    if not isinstance(data, dict) or data.get("schemaVersion") != 1:
        raise ValueError("invalid contract schema")
    modes = data.get("extendedHooks")
    if not isinstance(modes, list) or len(modes) != 5:
        raise ValueError("expected the five existing extended hook modes")
    if any(not isinstance(entry, dict) or type(entry.get("mode")) is not int for entry in modes) or sorted(entry["mode"] for entry in modes) != [1, 2, 3, 4, 5]:
        raise ValueError("hook mode identifiers must be unique and contiguous from 1 to 5")
    names = set()
    for entry in modes:
        name = entry.get("name")
        args = entry.get("parameters")
        result = entry.get("returnsInt32")
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9]{1,40}", name) or name in names:
            raise ValueError("invalid or duplicated hook name")
        if type(args) is not int or args not in range(4) or type(result) is not bool:
            raise ValueError("invalid hook signature")
        names.add(name)
    return sorted(modes, key=lambda mode: mode["mode"])


def render(modes: list[dict]) -> tuple[str, str]:
    modes = sorted(modes, key=lambda mode: mode["mode"])
    lines_cs = ["namespace BunnyLoader.Source;", "", "internal static class BunnyExtendedHookModes", "{", "    internal const int Min = 1;", "    internal const int Max = 5;"]
    lines_cpp = ["#pragma once", "#include <array>", "", "namespace bl::csharp {", "", "inline constexpr int kExtendedHookMin = 1;", "inline constexpr int kExtendedHookMax = 5;"]
    for entry in modes:
        name, mode = entry["name"], entry["mode"]
        lines_cs.append(f"    internal const int {name} = {mode};")
        lines_cpp.append(f"inline constexpr int kExtendedHook{name} = {mode};")
    lines_cs.extend(["}", ""])
    lines_cpp.extend(["", "struct ExtendedHookSignature {", "    int parameterCount;", "    bool returnsInt32;", "};", "", "inline constexpr std::array<ExtendedHookSignature, kExtendedHookMax + 1> kExtendedHookSignatures = {{", "    {0, false},"])
    lines_cpp.extend(f"    {{{entry['parameters']}, {'true' if entry['returnsInt32'] else 'false'}}}," for entry in modes)
    lines_cpp.extend(["}};", "", "}", ""])
    return "\n".join(lines_cs), "\n".join(lines_cpp)


def verify_source(modes: list[dict], root: Path = ROOT) -> None:
    sdk = (root / SDK.relative_to(ROOT)).read_text(encoding="utf-8")
    native = (root / CPP.relative_to(ROOT)).read_text(encoding="utf-8")
    for entry in modes:
        name = entry["name"]
        expected = "int" if entry["returnsInt32"] else "void"
        callback = re.search(rf"public delegate {expected} Bunny{name}Hook\s*\(([^)]*)\)", sdk)
        if not callback or callback.group(1).count(",") != 1 + entry["parameters"]:
            raise ValueError(f"C# delegate differs: {name}")
        if not re.search(rf"public BunnyMethodHook Hook{name}\s*\(", sdk):
            raise ValueError(f"C# registration missing: {name}")
        if f"case kExtendedHook{name}:" not in native:
            raise ValueError(f"native dispatch case missing: {name}")
        if f"BunnyExtendedHookModes.{name}" not in sdk:
            raise ValueError(f"managed hook mode not wired: {name}")
    if "kExtendedHookSignatures[install.extendedMode].parameterCount" not in native or "kExtendedHookSignatures[install.extendedMode].returnsInt32" not in native:
        raise ValueError("native ABI signature table not wired")


def generate(write: bool = False, root: Path = ROOT) -> None:
    modes = validate(json.loads((root / SPEC.relative_to(ROOT)).read_text(encoding="utf-8")))
    cs, cpp = render(modes)
    if root == ROOT:
        verify_source(modes, root)
    for path, content in ((root / CS_OUT.relative_to(ROOT), cs), (root / CPP_OUT.relative_to(ROOT), cpp)):
        if write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        elif not path.is_file() or path.read_text(encoding="utf-8") != content:
            raise ValueError(f"generated hook contract stale: {path.relative_to(root)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    generate(args.write)
