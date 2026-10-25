#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = Path(__file__).with_name("content_kinds.json")
CS_OUT = ROOT / "experiments/csharp/BunnyCsSdk/BunnyContentKind.g.cs"
CPP_OUT = ROOT / "app/src/main/cpp/experiments/csharp/SourceContentKinds.generated.h"
DECLARATION = ROOT / "experiments/csharp/BunnyCsSdk/BunnyContentDeclaration.cs"
PARSER = ROOT / "app/src/main/cpp/experiments/csharp/SourceParser.cpp"
LOADER = ROOT / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp"

EXPECTED = ("Item", "Tile", "Weapon", "Projectile", "Buff", "BuffPotion", "Accessory", "Player", "NPC", "Prefix", "Wall", "Dust", "Tree", "Gore", "Mount")
EXPECTED_ORDER = ('BunnyDust', 'BunnyTile', 'BunnyWall', 'BunnyItem', 'BunnyBuff', 'BunnyMount', 'BunnyProjectile', 'BunnyAccessory', 'BunnyNPC', 'BunnyBuffPotion', 'BunnyWeapon', 'BunnyPrefix')


def validate(data: dict) -> list[dict]:
    if not isinstance(data, dict) or data.get("schemaVersion") != 1:
        raise ValueError("unrecognized content contract schema")
    kinds = data.get("contentKinds")
    if not isinstance(kinds, list) or len(kinds) != len(EXPECTED):
        raise ValueError("missing or extra Bunny content category")
    if any(not isinstance(row, dict) for row in kinds):
        raise ValueError("invalid content descriptor")
    ordered = sorted(kinds, key=lambda row: row.get("value", -1) if type(row.get("value")) is int else -1)
    if tuple(row.get("kind") for row in ordered) != EXPECTED:
        raise ValueError("BunnyContentKind enum ABI has changed")
    priorities = []
    for row in ordered:
        if type(row["value"]) is not int or row["value"] != EXPECTED.index(row["kind"]):
            raise ValueError("BunnyContentKind enum ordinal has changed")
        if row.get("base") != "Bunny" + row["kind"]:
            raise ValueError("Bunny content kind no longer maps to its SDK base")
        priority = row.get("nativeOrder")
        if row["kind"] in ("Player", "Tree", "Gore"):
            if priority is not None:
                raise ValueError("BunnyPlayer must remain managed, not native registered")
        else:
            if type(priority) is not int or priority < 0 or priority >= len(EXPECTED) - 1:
                raise ValueError("invalid native registration priority")
            priorities.append((priority, row["base"]))
    if tuple(name for _, name in sorted(priorities)) != EXPECTED_ORDER or len(set(value for value, _ in priorities)) != len(priorities):
        raise ValueError("native category registration order differs from baseline")
    return ordered


def render(kinds: list[dict]) -> tuple[str, str]:
    kinds = sorted(kinds, key=lambda row: row["value"])
    native = sorted((row for row in kinds if row["nativeOrder"] is not None),
                    key=lambda row: row["nativeOrder"])
    managed = ["namespace BunnyLoader.Source;", "", "public enum BunnyContentKind", "{"]
    managed.extend(f'    {row["kind"]} = {row["value"]},' for row in kinds)
    managed.extend(["}", ""])
    header = ["#pragma once", "#include <array>", "#include <string_view>", "",
              "namespace bl::csharp {", ""]
    for identifier, entries, field in (
        ("kSourceContentKinds", kinds, "kind"),
        ("kSourceContentBases", kinds, "base"),
        ("kSourceNativeRegistrationOrder", native, "base"),
    ):
        header.append(f"inline constexpr std::array<const char*, {len(entries)}> {identifier} = {{{{")
        header.extend(f'    "{row[field]}",' for row in entries)
        header.extend(["}};", ""])
    header.extend([
        "inline bool isKnownSourceKind(std::string_view kind) {",
        "    for (const char* value : kSourceContentKinds)",
        "        if (kind == value) return true;",
        "    return false;",
        "}", "",
        "inline bool isRecognizedSourceBase(std::string_view base) {",
        "    for (const char* value : kSourceContentBases)",
        "        if (base == value) return true;",
        "    return false;",
        "}", "", "}", "",
    ])
    return "\n".join(managed), "\n".join(header)


def verify_integration(root: Path) -> None:
    decl = (root / DECLARATION.relative_to(ROOT)).read_text(encoding="utf-8")
    parser = (root / PARSER.relative_to(ROOT)).read_text(encoding="utf-8")
    loader = (root / LOADER.relative_to(ROOT)).read_text(encoding="utf-8")
    if re.search(r"public\s+enum\s+BunnyContentKind", decl):
        raise ValueError("duplicated C# content kind enum")
    if "SourceContentKinds.generated.h" not in parser or "isKnownSourceKind(tokens[i + 4].value)" not in parser:
        raise ValueError("C++ source parser is not using generated kind names")
    if parser.count("isRecognizedSourceBase(base)") < 3:
        raise ValueError("C++ source parser is not using generated base names")
    if "SourceContentKinds.generated.h" not in loader or "kSourceNativeRegistrationOrder" not in loader:
        raise ValueError("native content registration order is not generated")


def generate(write: bool = False, root: Path = ROOT) -> None:
    kinds = validate(json.loads((root / SPEC.relative_to(ROOT)).read_text(encoding="utf-8")))
    managed, native = render(kinds)
    verify_integration(root)
    for path, content in (
        (root / CS_OUT.relative_to(ROOT), managed),
        (root / CPP_OUT.relative_to(ROOT), native),
    ):
        if write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        elif not path.is_file() or path.read_text(encoding="utf-8") != content:
            raise ValueError(f"generated content contract stale: {path.relative_to(root)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    generate(args.write)
