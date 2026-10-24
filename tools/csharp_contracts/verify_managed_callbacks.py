#!/usr/bin/env python3
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = Path(__file__).with_name("managed_callbacks.json")
KINDS = Path(__file__).with_name("content_kinds.json")


def check() -> int:
    data = json.loads(SPEC.read_text(encoding="utf-8"))
    kinds = json.loads(KINDS.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1:
        raise ValueError("invalid managed callback schema")
    included = data.get("content")
    if not isinstance(included, list) or len(included) != 4:
        raise ValueError("expected four baseline content categories")
    if {row.get("type") for row in included} != {"BunnyItem", "BunnyProjectile", "BunnyNPC", "BunnyPlayer"}:
        raise ValueError("unexpected SDK callback categories")
    allowed = {row["base"] for row in kinds["contentKinds"]}
    count = 0
    for row in included:
        if row["type"] not in allowed:
            raise ValueError("callback group not backed by a declared Bunny content kind")
        sample = row.get("sample")
        if not isinstance(sample, str) or not sample.startswith("samples/ExampleModCSharp/Playable/content/") or ".." in Path(sample).parts or not sample.endswith(".cs"):
            raise ValueError("invalid sample path")
        path = ROOT / sample
        body = path.read_text(encoding="utf-8")
        if not re.search(r":\s*" + re.escape(row["type"]) + r"\b", body):
            raise ValueError("sample no longer extends its claimed Bunny base: " + sample)
        callbacks = row.get("callbacks")
        if not isinstance(callbacks, list) or not callbacks:
            raise ValueError("empty SDK callback contract")
        names = {m["name"] for m in callbacks}
        if len({(m["name"], tuple(m["args"])) for m in callbacks}) != len(callbacks):
            raise ValueError("duplicate callback signature")
        for method in row.get("sampleOverrides", []):
            if method not in names:
                raise ValueError("sample callback omitted from SDK contract: " + method)
            if not re.search(r"\boverride\s+\w+\s+" + re.escape(method) + r"\s*\(", body):
                raise ValueError("sample callback no longer overrides SDK: " + sample + " " + method)
        count += len(callbacks)
    return count


if __name__ == "__main__":
    print("MANAGED_CALLBACK_SOURCE_PASS " + str(check()))
