"""Classify trusted source refs without growing GitHub expression ASTs."""
import importlib.util
import json
import os
from pathlib import Path

POLICY = json.loads(Path(__file__).with_name("csharp-build-policy.json").read_text())


BATCH_BRANCH = "feat/csharp-summon-lifecycle-v95"


def batch_ready():
    path = Path(__file__).resolve().parents[1] / "loader_batch_gate.py"
    spec = importlib.util.spec_from_file_location("loader_batch_gate", path)
    gate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gate)
    return not gate.release_errors(json.loads(gate.PLAN.read_text()))


def classify(event, ref, head, head_repository, repository):
    branch = head if event == "pull_request" else (
        ref.removeprefix("refs/heads/") if ref.startswith("refs/heads/") else "")
    trusted = event != "pull_request" or (head_repository == repository and bool(repository))
    suffix = "_pr" if event == "pull_request" else "_push"
    result = {key: trusted and bool(branch) and branch in POLICY[key + suffix]
              for key in ("aot", "publish")}
    result["lights"] = trusted and bool(branch) and branch in POLICY["lights"]
    if result["publish"] and branch == BATCH_BRANCH:
        result["publish"] = batch_ready()
    return result


if __name__ == "__main__":
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    origin = event.get("pull_request", {}).get("head", {}).get("repo") or {}
    result = classify(os.environ["GITHUB_EVENT_NAME"], os.environ["GITHUB_REF"],
                      os.environ.get("GITHUB_HEAD_REF", ""), origin.get("full_name", ""),
                      os.environ["GITHUB_REPOSITORY"])
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        for key, enabled in result.items():
            output.write(key + "=" + str(enabled).lower() + "\n")
    print("CSHARP_BUILD_POLICY " + " ".join(key + "=" + str(value).lower()
                                            for key, value in result.items()))
