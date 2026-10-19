#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SDK_PATH = Path("docs/generated/sdk_public_reference.json")
MOD = Path("app/src/main/cpp/script/js/mod")
GROUPS = {
    "ItemLoader": {
        "source": "ModItem.js",
        "types": ("BunnyItem", "BunnyWeapon", "BunnyShootSource", "BunnyItemBehavior"),
        "callbacks": (
            "OnCraft", "CanUseItem", "UseItem", "CanRightClick", "RightClick",
            "ModifyTooltips", "HoldoutOffset", "Shoot", "OnHitNPC",
            "CanAccessoryBeEquippedWith",
        ),
    },
    "EquipLoader": {
        "source": "ModItem.js",
        "types": ("BunnyItemBehavior", "BunnyArmorSet", "BunnyPlayer", "BunnyVisual"),
        "callbacks": (
            "UpdateEquip", "UpdateAccessory", "UpdateVanity", "SetMatch",
            "EquipFrameEffects",
        ),
    },
    "ArmorSetLoader": {
        "source": "ModItem.js",
        "types": ("BunnyArmorSet", "BunnyArmorEffects", "BunnyPlayer"),
        "callbacks": (
            "AddArmorSets", "IsArmorSet", "UpdateArmorSet", "IsVanitySet",
            "ArmorSetShadows", "SetMatch", "EquipFrameEffects",
        ),
    },
    "WingLoader": {
        "source": "ModItem.js",
        "types": ("BunnyItemBehavior", "BunnyPlayer"),
        "callbacks": (
            "SetWingStats", "VerticalWingSpeeds", "HorizontalWingSpeeds",
            "WingUpdate",
        ),
    },
    "PrefixLoader": {
        "source": "ModPrefix.js",
        "types": ("BunnyPrefix",),
        "callbacks": (
            "SetStaticDefaults", "RollChance", "CanRoll", "SetStats",
            "AllStatChangesHaveEffectOn", "Apply", "ModifyValue",
            "ApplyAccessoryEffects", "GetTooltipLines",
        ),
    },
}
METHOD = re.compile(r"^    (?:(?:static|get|set|async) )*([A-Za-z_]\w*)\s*\(")


def checked(root, relative):
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError("Original or SDK source missing: " + str(relative))
    return path


def hash_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def js_public_methods(path):
    found = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = METHOD.match(line)
        if match:
            found.setdefault(match.group(1), number)
    return found


def sdk_methods(type_spec):
    methods = {}
    for entry in type_spec.get("methods", []):
        signature = entry["signature"].split("(", 1)[0]
        method = signature.rsplit(" ", 1)[-1]
        methods[method] = {"signature": entry["signature"], "execution": entry.get("execution", "unspecified")}
    return methods


def audit(root=ROOT, groups=GROUPS):
    root = Path(root).resolve()
    sdk_source = checked(root, SDK_PATH)
    sdk = json.loads(sdk_source.read_text(encoding="utf-8"))
    known_types = {spec["name"].rsplit(".", 1)[-1]: spec for spec in sdk["types"]}
    report = []
    for loader, group in groups.items():
        relative = MOD / group["source"]
        source = checked(root, relative)
        js = js_public_methods(source)
        referenced_sdk = {}
        for type_name in group["types"]:
            if type_name not in known_types:
                raise ValueError("Missing SDK type " + type_name + " required by " + loader)
            referenced_sdk[type_name] = sdk_methods(known_types[type_name])
        callbacks = []
        for method in group["callbacks"]:
            if method not in js:
                raise ValueError(loader + ": original callback missing: " + method)
            matching_names = [
                {"type": type_name, **members[method]}
                for type_name, members in referenced_sdk.items() if method in members
            ]
            callbacks.append({
                "original": method,
                "originalLine": js[method],
                "sdkSameName": matching_names,
                "sdkNamePresent": bool(matching_names),
                "semanticParityConfirmed": False,
                "requiresNativeAndDeviceReview": True,
            })
        report.append({
            "loader": loader,
            "originalSource": relative.as_posix(),
            "originalSha256": hash_file(source),
            "sdkTypesReviewed": list(group["types"]),
            "callbacks": callbacks,
        })
    return {
        "schemaVersion": 1,
        "scope": "equipment-loader-source-surface",
        "meaning": "Lexical source-to-public-SDK name inventory only, never proof of callback execution, semantic parity or Android validation.",
        "sdkReference": SDK_PATH.as_posix(),
        "sdkReferenceSha256": hash_file(sdk_source),
        "loaders": report,
    }


def render_markdown(report):
    lines = [
        "# Superfície de callbacks — equipamentos",
        "",
        "Nomes iguais entre JS e C# indicam somente uma pista lexical, não equivalência de execução. Nomes diferentes não provam ausência de função nativa.",
        "",
        "Fonte do SDK compilado: `" + report["sdkReference"] + "`",
        "",
    ]
    for item in report["loaders"]:
        lines.extend([
            "## " + item["loader"],
            "",
            "Original: `" + item["originalSource"] + "`",
            "",
            "| Callback original | Linha JS | Nome no SDK C# | Evidência |",
            "| --- | ---: | --- | --- |",
        ])
        for entry in item["callbacks"]:
            hints = ", ".join(x["type"] + "." + entry["original"] + " (" + x["execution"] + ")"
                              for x in entry["sdkSameName"])
            lines.append(
                "| `" + entry["original"] + "` | " + str(entry["originalLine"]) +
                " | " + ("Mesmo nome" if hints else "Sem nome idêntico") + " | " +
                (hints or "Revisar implementação nativa/gerenciada") + " |"
            )
        lines.append("")
    lines.append("Todos os callbacks listados exigem revisão manual do fluxo real, testes por categoria e validação no Android. Nenhum loader é promovido a completo.")
    return "\n".join(lines) + "\n"


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument("--json", action="store_true")
    cli.add_argument("--output", type=Path)
    args = cli.parse_args()
    report = audit()
    body = json.dumps(report, indent=2, ensure_ascii=False) + "\n" if args.json else render_markdown(report)
    if args.output:
        args.output.write_text(body, encoding="utf-8")
    else:
        print(body, end="")


if __name__ == "__main__":
    main()
