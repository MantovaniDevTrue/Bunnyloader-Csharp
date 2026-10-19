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
            assert result[key] == (branch in module.POLICY[key + ("_pr" if event == "pull_request" else "_push")] and not (key == "publish" and branch == module.BATCH_BRANCH and not module.batch_ready()))
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

# User requires five complete loaders per next APK; the active batch is incomplete.
for event in ("push", "workflow_dispatch", "pull_request"):
    held=module.classify(event,"refs/heads/feat/csharp-summon-lifecycle-v95","feat/csharp-summon-lifecycle-v95",repo,repo)
    assert held["aot"] and held["lights"] and held["publish"] == module.batch_ready()
assert "needs.build.outputs.publish == 'true'" in workflow

# A direct call must also stop before credentials/network when publication is held.
for ref, head in (("refs/heads/feat/csharp-summon-lifecycle-v95", ""), ("refs/pull/98/merge", "feat/csharp-summon-lifecycle-v95")):
    result=subprocess.run(["bash", str(root/"tools/ci/publish_csharp_private.sh")],
        env=dict(os.environ,GITHUB_REF=ref,GITHUB_HEAD_REF=head,GH_TOKEN=""),capture_output=True,text=True)
    assert result.returncode!=0 and ("missing" if module.batch_ready() else "PublicationHeldFiveLoaderBatch") in result.stdout

# Completion changes publication readiness, without bypassing repository trust.
original_ready = module.batch_ready
for ready in (False, True):
    module.batch_ready = lambda: ready
    assert module.classify("push", "refs/heads/" + module.BATCH_BRANCH, "", repo, repo)["publish"] == ready
    assert module.classify("pull_request", "refs/pull/98/merge", module.BATCH_BRANCH, repo, repo)["publish"] == ready
    assert not module.classify("pull_request", "refs/pull/98/merge", module.BATCH_BRANCH, "fork/repo", repo)["publish"]
module.batch_ready = original_ready
