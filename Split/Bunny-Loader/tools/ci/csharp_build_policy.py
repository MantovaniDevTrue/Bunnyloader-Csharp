"""Classify trusted source refs without growing GitHub expression ASTs."""
import json
import os
from pathlib import Path

POLICY = json.loads(Path(__file__).with_name("csharp-build-policy.json").read_text())


def classify(event, ref, head, head_repository, repository):
    if event == "pull_request":
        trusted = head_repository == repository and bool(repository)
        return {key: trusted and head in POLICY[key + "_pr"]
                for key in ("aot", "publish")} | {
                    "lights": trusted and head in POLICY["lights"]}
    branch = ref.removeprefix("refs/heads/") if ref.startswith("refs/heads/") else ""
    return {key: bool(branch) and branch in POLICY[key + "_push"]
            for key in ("aot", "publish")} | {"lights": branch in POLICY["lights"]}


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
