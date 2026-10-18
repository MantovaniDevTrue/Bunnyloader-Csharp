import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
path = root / "tools/ci/csharp_build_policy.py"
spec = importlib.util.spec_from_file_location("build_policy", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
repo = "MantovaniDevTrue/Bunnyloader-Csharp"
native_branch = "feat/csharp-94-owned-summon-damage-lot"
for key in ("aot_push", "publish_push", "lights"):
    assert native_branch in module.POLICY[key], f".90.4 missing trusted {key} policy"
for workflow_name in ("check-mono-pinned-gc.yml", "check-mono-signal-ownership.yml"):
    workflow_text = (root.parents[1] / ".github/workflows" / workflow_name).read_text()
    assert native_branch in workflow_text, f".90.4 compiler delivery gate missing {workflow_name}"
assert module.classify("push", "refs/heads/fix/csharp-90-1-static-recipegroup-register", "", repo, repo)["publish"]
assert module.classify("push", "refs/heads/fix/csharp-90-1-static-recipegroup-register", "", repo, repo)["aot"]
for event in ("push", "workflow_dispatch", "pull_request"):
    for branch in set().union(*map(set, module.POLICY.values())) | {"unknown", "main"}:
        result = module.classify(event, "refs/heads/" + branch, branch, repo, repo)
        for key in ("aot", "publish"):
            assert result[key] == (branch in module.POLICY[key + ("_pr" if event == "pull_request" else "_push")])
        assert result["lights"] == (branch in module.POLICY["lights"])
        assert not any(module.classify("pull_request", "refs/pull/81/merge", branch, "fork/repo", repo).values())
assert all(module.classify("push", "refs/heads/feat/csharp-loader-foundation-v91", "", "", repo).values())
assert not any(module.classify("push", "refs/tags/fix/csharp-88-8-lights-review", "", "", repo).values())
assert not any(module.classify("pull_request", "refs/pull/81/merge", "fix/csharp-88-8-lights-review", "", repo).values())
with tempfile.TemporaryDirectory() as directory:
    event, output = Path(directory) / "event.json", Path(directory) / "output"
    event.write_text(json.dumps({"pull_request": {"head": {"repo": {"full_name": repo}}}}))
    env = dict(os.environ, GITHUB_EVENT_PATH=str(event), GITHUB_OUTPUT=str(output),
               GITHUB_EVENT_NAME="pull_request", GITHUB_REF="refs/pull/81/merge",
               GITHUB_HEAD_REF="fix/csharp-88-8-lights-review", GITHUB_REPOSITORY=repo)
    subprocess.run(["python3", str(path)], env=env, check=True)
    assert output.read_text() == "aot=true\npublish=true\nlights=true\n"
workflow = (root.parents[1] / ".github/workflows/build-bunny-csharp-source-loader.yml").read_text()
expressions = re.findall(r"\$\{\{(.*?)\}\}", workflow, re.S)
assert max(map(len, expressions)) < 2000, "GitHub expression growth: move branch policy into tested Python"
gate = re.search(r"contains\(fromJSON\('(.*?)'\), github.head_ref\)", workflow)
assert gate and json.loads(gate[1]) == module.POLICY["android_pr"]
print("CSHARP_BUILD_POLICY_EVENTS_REFS_FORKS_OUTPUT_AND_EXPRESSION_LIMIT=PASS")

# GitHub also limits the expanded literal of each interpolated step, not only expressions.
run_blocks=re.findall(r"(?m)^        run: \|\n((?: {10}.*\n|\n)*)",workflow)
assert max((len(block) for block in run_blocks if '${{' in block),default=0)<18000, 'Extract large interpolated run steps before GitHub 21000 limit'
