#!/usr/bin/env python3
"""Audita fontes do ExampleMod CSharp 0.1.63 sem compilar .bmod nem APK.

O .bl e um ZIP de fontes. O checker nunca extrai caminhos do arquivo:
le apenas bytes de membros content/*.cs, compara blobs Git e verifica
se cada fonte ja existe no Playable ou foi preservada em Porting.

A identidade de SHA Git confere exatamente os bytes (inclusive acentos
e quebras de linha) da versao enviada, sem trocar texturas ou callbacks.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from zipfile import BadZipFile, ZipFile

ROOT = Path(__file__).resolve().parents[2]
PACKAGE_SHA256 = "c8a469f7f45e92349d5a16978cac9a62127a5f1fe0bd46bdee7e4c7700e8ef15"
PACKAGE_VERSION = "0.1.63"
PACKAGE_CSHARP = 98
STAGING = "samples/ExampleModCSharp/Porting/Archive-0.1.63/content"
PLAYABLE = "samples/ExampleModCSharp/Playable/content"

# Os hashes foram conferidos contra o pacote fornecido e os objetos Git
# existentes nas PRs C# do proprio Bunny, nao derivados de APIs do tML.
STAGED_BLOBS = {
    "Common/Audio/ExampleSoundEffects.cs": "e05d83de9734d4c296d459aca838503cb31894df",
    "Common/Commands/ExampleEchoCommand.cs": "4264527a45ceaf044cf17168fe5c7963d76fb45a",
    "Common/Commands/ExampleHealCommand.cs": "72b82740ea162c354c753000bbfceae75eb03020",
    "Common/Commands/ExampleTimeCommand.cs": "ee68ece4a9f95a2295878156d40927f6233e54d9",
    "Common/Localization/ExampleLocalizationModule.cs": "b844ee6847a5b36154d7e28c88d030cfcc48171f",
    "Common/Players/ExampleFishingPlayer.cs": "24f7029971880357ee707811ffd9326b06d9e7db",
    "Content/Effects/ExampleSparkDust.cs": "d638e8964d2c7732eb2e2f221d766f31c58de4ff",
    "Content/Items/Consumables/ExampleInvasionBeacon.cs": "a54298a2f1399ab89560597935c36fe3d3b9c8a6",
    "Content/Items/ExampleDustCharm.cs": "768e956023a7f7889ea6761e69160c00c16314a7",
    "Content/Items/ExampleTooltipItem.cs": "a0f732fa98efb5d1c4f57aacade8f6d32687196d",
    "Content/Items/Placeable/ExampleWallAdvanced.cs": "5e1f36b50591f0cfe27cb956c61d0607718dc44f",
    "Content/Items/Quests/ExampleQuestFish.cs": "c3a43a02b9f966fe3a84fed827f0781be984bda8",
    "Content/Items/Weapons/Ranged/ExampleShotgun.cs": "e9bb04e2e3f492abf1dea742b3849223d9f89469",
    "Content/NPCs/ExampleInvasionEliteSlimeNPC.cs": "d48ed2d14273f83d3d3fa7ff70cc93354a8ae407",
    "Content/Rarities/ExampleHigherTierModRarity.cs": "484915fa7cf1be90a3a91f127b5025bbc8770e65",
    "Content/Rarities/ExampleModRarity.cs": "1a763e4b9329f29710dccde1ac727c55ad424928",
    "Content/Systems/ExampleInvasion.cs": "33d97f8aac8db369e1fc2929dda8913cc1cffdcd",
    "Content/Systems/ExampleInvasionSystem.cs": "fcc265aea852b641269f81cd53eae430995246c7",
    "Content/Walls/ExampleWallAdvanced.cs": "40fe30590d59e94dd568564862a660a76f09a33a",
}


def blob_sha(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode("ascii") + b"\x00" + data
    ).hexdigest()


def safe_member(name: str) -> bool:
    path = PurePosixPath(name)
    return bool(name) and "\\" not in name and not path.is_absolute() and (
        ".." not in path.parts and path.suffix == ".cs" and path.parts[0] == "content"
    )


def check_staged(root: Path) -> list[str]:
    errors = []
    for relative, sha in STAGED_BLOBS.items():
        staged = root / STAGING / relative
        if not staged.is_file():
            errors.append(f"Fonte 0.1.63 ausente: {relative}")
            continue
        if blob_sha(staged.read_bytes()) != sha:
            errors.append(f"Fonte 0.1.63 alterada: {relative}")
        # Porting e intencional: classes que exigem novas APIs ainda nao
        # podem ser autocarregadas no Playable com um comportamento falso.
        if (root / PLAYABLE / relative).is_file():
            errors.append(f"Fonte duplicada em Playable antes da promocao: {relative}")
    return errors


def check_archive(root: Path, archive: Path) -> tuple[list[str], dict]:
    errors = []
    details: dict[str, object] = {}
    if not archive.is_file():
        return ["Arquivo .bl nao encontrado"], details
    if hashlib.sha256(archive.read_bytes()).hexdigest() != PACKAGE_SHA256:
        return ["O pacote .bl nao e a versao enviada 0.1.63"], details
    try:
        with ZipFile(archive) as zipfile:
            files = [info for info in zipfile.infolist() if info.filename.endswith(".cs")]
            if len(files) != PACKAGE_CSHARP:
                errors.append(f"Quantidade de C# diferente de {PACKAGE_CSHARP}")
            seen = set()
            current = []
            identical = 0
            changed = []
            for info in files:
                member = info.filename
                if not safe_member(member) or member in seen or info.file_size > 2_000_000:
                    errors.append(f"Nome ou tamanho invalido no arquivo: {member}")
                    continue
                seen.add(member)
                relative = member.removeprefix("content/")
                data = zipfile.read(info)
                expected = STAGED_BLOBS.get(relative)
                if expected:
                    if blob_sha(data) != expected:
                        errors.append(f"Snapshot Git nao corresponde ao .bl: {relative}")
                    continue
                playable = root / PLAYABLE / relative
                if not playable.is_file():
                    errors.append(f"Fonte de exemplo ainda ausente em Playable: {relative}")
                    continue
                current.append(relative)
                if blob_sha(playable.read_bytes()) == blob_sha(data):
                    identical += 1
                else:
                    changed.append(relative)
            details = {
                "packageVersion": PACKAGE_VERSION,
                "archiveClasses": len(files),
                "stagedExact": len(STAGED_BLOBS),
                "playablePresent": len(current),
                "playableIdenticalToArchive": identical,
                "playableChanged": changed,
                "apkGenerated": False,
            }
    except (BadZipFile, OSError, RuntimeError, ValueError) as exc:
        errors.append(f"Falha ao ler fontes do pacote: {type(exc).__name__}")
    return errors, details


def main() -> int:
    parser = argparse.ArgumentParser(description="Auditoria de fontes Bunny 0.1.63, sem APK")
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--archive", type=Path)
    args = parser.parse_args()
    errors = check_staged(args.project_root)
    report = {"staged": len(STAGED_BLOBS), "apkGenerated": False}
    if args.archive:
        found, details = check_archive(args.project_root, args.archive)
        errors.extend(found)
        report.update(details)
    if errors:
        print("EXAMPLE_CSHARP_ARCHIVE_GATE=BLOCKED")
        print(json.dumps({"errors": errors[:60], **report}, ensure_ascii=False, indent=2))
        return 1
    print("EXAMPLE_CSHARP_ARCHIVE_GATE=PASS")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
