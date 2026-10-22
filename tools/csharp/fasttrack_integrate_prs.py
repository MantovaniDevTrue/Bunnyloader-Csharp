#!/usr/bin/env python3
"""Conservative, reproducible three-way source integration for Bunny C# fast-track."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE = "cfbe784ee3cbc0ba2fabafce56c59e6eac41b67b"
SOURCES = (
    (117, "a32d159bb913dae508af503da375765c855d96bc", "CommandLoader"),
    (121, "d061f7654660223ea00e55c7d21d85dfdffcb8d9", "GoreLoader"),
)
REPORT = ROOT / "docs/csharp/FASTTRACK-INTEGRATION-REPORT.json"
ALLOWED = (".cs", ".csproj", ".cpp", ".h", ".hpp", ".c", ".cmake", ".json", ".md", ".png", ".xml", ".props", ".targets")
ROOTS = ("app/", "experiments/", "samples/", "tests/", "tools/", "docs/", "Split/Bunny-Loader/")
MAX_BYTES = 5_000_000
MANUAL_RECONCILIATIONS = {
    (117, "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp"):
        (b'#include "experiments/csharp/NativeCommandManifest.h"',
         b'bindManagedCommandMod(mod.id, assembly, &commandError)',
         b'installNativeCommandChatHooks()', b'nativeCommandMailbox().newWorld()'),
    (117, "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp"):
        (b'bool prepareManagedCommandWorker()', b'executeManagedCommandOnWorker',
         b'nativeCommandMailbox().takeRequests(8)', b'bool bindManagedCommandMod('),
    (121, "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp"):
        (b'gore::worldBoundary()', b'gore::worldReady()', b'gore::pumpOnGameThread()'),
    (121, "app/src/main/cpp/experiments/csharp/SourceLoader.cpp"):
        (b'gore::discover(mod.dir', b'gore::install(mod.id',
         b'status->goreSourcesDiscovered'),
    (121, "app/src/main/cpp/experiments/csharp/SourceLoader.h"):
        (b'int goreSourcesDiscovered = 0;',),
    (117, "app/src/main/cpp/experiments/csharp/ManagedSourceClassCount.h"):
        (b"mod.screenEffects + mod.commands",),
    (117, "app/src/main/cpp/experiments/csharp/SourceParser.cpp"):
        (b'base != "BunnyCommand"', b'if (base == "BunnyCommand")'),
    (117, "app/src/main/cpp/experiments/csharp/SourceLoader.cpp"):
        (b'status->commandClasses.push_back', b'status->commands > 128',
         b'status->commands==0', b'hasNativeContent && !prepareNativeContent()'),
    (117, "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs"):
        (b"CommandManifestWriter.CountCommands(assembly)", b"CommandManifestWriter.Write(assembly, diagnosticsPath, assemblyPath)"),
    (117, "experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj"):
        (b'BunnyCommand.cs', b'BunnyCommandWorker.cs'),
    (121, "experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj"):
        (b'BunnyGore.cs',),
}

def git(*args, check=True):
    result = subprocess.run(["git", *args], cwd=ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False)
    if check and result.returncode:
        raise RuntimeError("git " + " ".join(args) + ": " + result.stderr.decode("utf-8", "replace")[:1000])
    return result

def source_at(rev, path):
    result = git("show", rev + ":" + path, check=False)
    if result.returncode:
        return None
    return result.stdout

def sha(data):
    return hashlib.sha256(data).hexdigest() if data is not None else None

def safe_path(path):
    return (
        path.startswith(ROOTS)
        and not path.startswith("docs/csharp/FASTTRACK-INTEGRATION-REPORT")
        and not path.endswith((".apk", ".so", ".a", ".dll", ".bmod", ".tmod", ".tl", ".js", ".mjs"))
        and (path.endswith(ALLOWED) or path.endswith("CMakeLists.txt"))
        and not path.startswith("app/build/")
        and not path.startswith("app/src/main/assets/managed/")
        and ".." not in Path(path).parts
    )

def merge(base, ours, theirs):
    if ours == theirs:
        return ours, "already_equal"
    if ours == base:
        return theirs, "clean_take_pr"
    if theirs == base:
        return ours, "already_contains_pr"
    if ours is None or theirs is None or base is None:
        return ours, "delete_or_add_collision"
    if any(b"\x00" in part for part in (base, ours, theirs)):
        return ours, "binary_collision"
    with tempfile.TemporaryDirectory() as tmp:
        files = [Path(tmp) / key for key in ("current", "original", "incoming")]
        for p, data in zip(files, (ours, base, theirs)):
            p.write_bytes(data)
        outcome = subprocess.run(
            ["git", "merge-file", "--diff3", "-p", str(files[0]), str(files[1]), str(files[2])],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
    if outcome.returncode == 0:
        return outcome.stdout, "clean_three_way"
    if 1 <= outcome.returncode <= 127:
        return ours, "three_way_conflict"
    raise RuntimeError("git merge-file failed: " + outcome.stderr.decode("utf-8", "replace")[:500])

def main():
    assert git("rev-parse", "--is-inside-work-tree").stdout.strip() == b"true"
    assert git("status", "--porcelain").stdout.strip() == b"", "Refuse dirty working tree"
    assert git("cat-file", "-t", BASE).stdout.strip() == b"commit"
    results = []
    pending = []
    counts = {}
    for number, expected, name in SOURCES:
        ref = "refs/remotes/csharp-fasttrack/pr" + str(number)
        git("fetch", "--no-tags", "origin", "refs/pull/%d/head:%s" % (number, ref))
        head = git("rev-parse", ref).stdout.decode().strip()
        assert head == expected, "PR #%d changed: %s != %s" % (number, head, expected)
        ancestor = git("merge-base", BASE, head).stdout.decode().strip()
        assert ancestor == BASE, "Unexpected source history for PR #%d" % number
        changes = git("diff", "--name-only", "--no-renames", "-z", BASE, head).stdout
        files = [entry.decode("utf-8", "strict") for entry in changes.split(b"\x00") if entry]
        stats = {"clean_take_pr": 0, "clean_three_way": 0, "already_equal": 0,
                 "already_contains_pr": 0, "manual_reconciled": 0, "blocked": 0, "excluded": 0}
        for path in files:
            if not safe_path(path):
                stats["excluded"] += 1
                pending.append({"pr": number, "path": path, "reason": "outside_safe_source_scope"})
                continue
            original = source_at(BASE, path)
            incoming = source_at(head, path)
            dest = ROOT / path
            current = dest.read_bytes() if dest.is_file() else None
            if any(len(part) > MAX_BYTES for part in (original, incoming, current) if part is not None):
                stats["blocked"] += 1
                pending.append({"pr": number, "path": path, "reason": "size_limit"})
                continue
            merged, reason = merge(original, current, incoming)
            if reason == "three_way_conflict" and current is not None:
                markers = MANUAL_RECONCILIATIONS.get((number, path), ())
                if markers and all(marker in current for marker in markers):
                    stats["manual_reconciled"] += 1
                    results.append({"pr": number, "path": path,
                                    "result": "manual_source_reconciled",
                                    "sha256": sha(current)})
                    continue
            if reason in ("three_way_conflict", "delete_or_add_collision", "binary_collision"):
                stats["blocked"] += 1
                pending.append({"pr": number, "path": path, "reason": reason,
                                "base_sha256": sha(original), "local_sha256": sha(current),
                                "incoming_sha256": sha(incoming)})
                continue
            stats[reason] += 1
            if merged != current:
                if merged is None:
                    dest.unlink()
                else:
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(merged)
            results.append({"pr": number, "path": path, "result": reason, "sha256": sha(merged)})
        counts[name] = stats
    report = {
        "schema": 1,
        "status": "SOURCE_ONLY_PENDING_ANDROID_AND_CONFLICT_RECONCILIATION",
        "integration_base": "PR #119, commit 489ec3e7b88b0e4540446a7cc404724e68408427",
        "common_base": BASE,
        "source_prs": [{"number": n, "sha": h, "name": name} for n, h, name in SOURCES],
        "counts": counts,
        "integrated": results,
        "manual_reconciliation": pending,
        "rules": {
            "no_tmodloader_code": True,
            "no_js_runtime_dependency": True,
            "no_apk_or_release": True,
            "no_last_writer_wins": True,
            "not_loader_certification": True,
        },
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("SOURCE_FASTTRACK=STAGED")
    print("CHANGED_SAFE=%d MANUAL=%d" % (
        sum(1 for r in results if r["result"] in ("clean_take_pr", "clean_three_way")), len(pending)))
    print(json.dumps(counts, sort_keys=True))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
