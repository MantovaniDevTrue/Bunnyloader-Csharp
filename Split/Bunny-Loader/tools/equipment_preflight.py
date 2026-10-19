#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = Path("samples/ExampleMod/content")
PORT = Path("samples/ExampleModCSharp/Playable/content")
MODULES = Path("app/src/main/cpp/script/js/mod/Loaders")
SDK_REFERENCE = Path("docs/generated/sdk_public_reference.json")
BATCH = "loaders-02"

SPECS = {
    "ItemLoader": {
        "sdk": ("BunnyItem", "BunnyWeapon"),
        "originals": ("Content/Items/Weapons/Ranged/ExampleGun.js",),
        "ports": ("Content/Items/Weapons/Ranged/ExampleGun.cs",),
        "symbols": ("SetDefaults", "HoldoutOffset"),
    },
    "EquipLoader": {
        "sdk": ("BunnyItemBehavior", "BunnyEquipKind"),
        "originals": ("Content/Items/Accessories/ExampleBeard.js",),
        "ports": ("Content/Items/Accessories/ExampleBeard.cs",),
        "symbols": ("SetStaticDefaults", "SetDefaults"),
    },
    "ArmorSetLoader": {
        "sdk": ("BunnyArmorSet", "BunnyArmorEffects"),
        "originals": (
            "Content/Items/Armor/ExampleHelmet.js",
            "Content/Items/Armor/ExampleBreastplate.js",
            "Content/Items/Armor/ExampleLeggings.js",
        ),
        "ports": (
            "Content/Items/Armor/ExampleHelmet.cs",
            "Content/Items/Armor/ExampleBreastplate.cs",
            "Content/Items/Armor/ExampleLeggings.cs",
        ),
        "symbols": ("AddArmorSets", "UpdateArmorSet", "UpdateEquip", "ModifyTooltipLines"),
    },
    "WingLoader": {
        "sdk": ("BunnyItemBehavior", "BunnyEquipKind"),
        "originals": ("Content/Items/Accessories/ExampleWings.js",),
        "ports": ("Content/Items/Accessories/ExampleWings.cs",),
        "symbols": ("SetWingStats", "VerticalWingSpeeds", "AddRecipes"),
    },
    "PrefixLoader": {
        "sdk": ("BunnyPrefix",),
        "originals": (
            "Content/Prefixes/ExamplePrefix.js",
            "Content/Prefixes/ExampleAccessoryPrefix.js",
        ),
        "ports": (
            "Content/Prefixes/ExampleMightyPrefix.cs",
            "Content/Prefixes/ExampleGuardedPrefix.cs",
        ),
        "symbols": (
            "RollChance", "CanRoll", "SetStats", "ModifyValue",
            "Apply", "GetTooltipLines", "ApplyAccessoryEffects",
        ),
    },
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def anchored_file(root, relative):
    target = (root / relative).resolve()
    if not target.is_relative_to(root.resolve()) or not target.is_file():
        raise ValueError("Source missing or outside repository: " + str(relative))
    return target


def read_json(root, relative):
    return json.loads(anchored_file(root, relative).read_text(encoding="utf-8"))


def symbol_present(source, name):
    return re.search(r"\b" + re.escape(name) + r"\s*\(", source) is not None


def build_report(root=ROOT, specs=SPECS):
    root = Path(root).resolve()
    plan_path = root.parents[1] / "docs/csharp-loader-batches.json"
    if not plan_path.is_file():
        raise ValueError("Equipment batch plan not found")
    plan = json.loads(plan_path.read_text(encoding="utf-8"))
    batches = [b for b in plan["batches"] if b["id"] == BATCH]
    if len(batches) != 1:
        raise ValueError("Equipment batch not found or duplicated")
    entries = batches[0]["loaders"]
    names = [e["loader"] for e in entries]
    if len(names) != 5 or len(set(names)) != 5 or set(names) != set(specs):
        raise ValueError("Equipment batch must contain exactly the five planned loaders")
    sdk = read_json(root, SDK_REFERENCE)
    public_types = {t["name"].rsplit(".", 1)[-1] for t in sdk["types"]}
    items = []
    for entry in entries:
        name = entry["loader"]
        spec = specs[name]
        modules = []
        if not entry["modules"]:
            raise ValueError("Unaccounted equipment modules: " + name)
        for module in entry["modules"]:
            relative = MODULES / (module + ".js")
            path = anchored_file(root, relative)
            modules.append({"path":relative.as_posix(),"sha256":sha256(path)})
        originals = []
        joined = []
        for relative in spec["originals"]:
            file = ORIGINAL / relative
            path = anchored_file(root, file)
            originals.append({"path":file.as_posix(),"sha256":sha256(path)})
            joined.append(path.read_text(encoding="utf-8"))
        ports = []
        for relative in spec["ports"]:
            file = PORT / relative
            path = anchored_file(root, file)
            ports.append({"path":file.as_posix(),"sha256":sha256(path)})
        text = "\n".join(joined)
        items.append({
            "loader":name,
            "contractStatus":entry["contractStatus"],
            "exampleStatus":entry["exampleStatus"],
            "modules":modules,
            "originalExamples":originals,
            "existingCSharpExamples":ports,
            "sdkTypes":{name: name in public_types for name in spec["sdk"]},
            "originalSymbols":{symbol: symbol_present(text,symbol) for symbol in spec["symbols"]},
            "reviewRequired":True,
            "deviceVerifiedByThisReport":False,
        })
    return {
        "schemaVersion":1,
        "batch":BATCH,
        "meaning":"Source existence and lexical anchors only; no CSharp loader parity or Android validation is inferred.",
        "loaders":items,
    }


def as_markdown(report):
    lines = [
        "# Pré-auditoria do lote de equipamentos",
        "",
        "Este relatório confere fontes e nomes. Não certifica hooks, comportamento, FPS nem compatibilidade Android.",
        "",
        "| Loader | Módulos JS | Exemplos JS | Fontes C# | API SDK presente | Estado do plano |",
        "| --- | ---: | ---: | ---: | --- | --- |",
    ]
    for entry in report["loaders"]:
        api = ", ".join(k + (" ✓" if v else " ausente") for k,v in entry["sdkTypes"].items())
        lines.append(
            f"| {entry['loader']} | {len(entry['modules'])} | {len(entry['originalExamples'])} | "
            f"{len(entry['existingCSharpExamples'])} | {api} | {entry['contractStatus']} |"
        )
    for entry in report["loaders"]:
        lines.extend(["", "## " + entry["loader"], "",
                      "**Símbolos encontrados nos exemplos JS (pistas, não equivalências C#):** " +
                      ", ".join(k + (" ✓" if v else " não encontrado") for k,v in entry["originalSymbols"].items()),
                      "", "**Arquivos-fonte conferidos:**"])
        for source in entry["modules"] + entry["originalExamples"] + entry["existingCSharpExamples"]:
            lines.append("- `" + source["path"] + "` — SHA-256 `" + source["sha256"] + "`")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = build_report()
    content = (json.dumps(report,ensure_ascii=False,indent=2)+"\n") if args.json else as_markdown(report)
    if args.output:
        args.output.write_text(content,encoding="utf-8")
    else:
        print(content,end="")


if __name__ == "__main__":
    main()
