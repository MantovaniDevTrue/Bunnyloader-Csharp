"""Refuse experimental compiler delivery until all independent regressions pass."""
import json
import sys

REQUIRED = {
    "Check CSharp SDK and continuous runtime foundation",
    "Check CSharp combat batch", "Check managed callback lifecycle",
    "Check complete ExampleMod audit", "Check Bunny CSharp Native Visual Queue",
    "Check Bunny CSharp Visual Queue", "Check Mono Android signal ownership",
    "Check Mono pinned worker GC transitions",
}


def check(payload, sha):
    latest = {}
    for run in payload["workflow_runs"]:
        if run["head_sha"] != sha or run["name"] not in REQUIRED:
            continue
        if run["name"] not in latest or run["id"] > latest[run["name"]]["id"]:
            latest[run["name"]] = run
    failed = [name for name, run in latest.items()
              if run["status"] == "completed" and run["conclusion"] != "success"]
    if failed:
        print("COMPILER_DELIVERY_REJECTED Failed=" + ",".join(sorted(failed)))
        return 1
    pending = REQUIRED - {name for name, run in latest.items()
                          if run["status"] == "completed" and run["conclusion"] == "success"}
    if pending:
        print("COMPILER_DELIVERY_WAIT Pending=" + ",".join(sorted(pending)))
        return 2
    print("COMPILER_DELIVERY_REGRESSIONS=PASS workflows=" + str(len(REQUIRED)))
    return 0


if __name__ == "__main__":
    sys.exit(check(json.loads(open(sys.argv[1]).read()), sys.argv[2]))
