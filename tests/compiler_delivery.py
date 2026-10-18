import contextlib
import importlib.util
import io
from pathlib import Path

spec = importlib.util.spec_from_file_location("delivery", Path(__file__).parents[1] / "tools/check_compiler_delivery.py")
delivery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(delivery)
rows = [{"id": index, "name": name, "head_sha": "current", "status": "completed", "conclusion": "success"}
        for index, name in enumerate(sorted(delivery.REQUIRED))]
def result(runs):
    with contextlib.redirect_stdout(io.StringIO()):
        return delivery.check({"workflow_runs": runs}, "current")
assert result(rows) == 0
assert result(rows[:-1]) == 2
assert result([dict(row, head_sha="old") for row in rows]) == 2
assert result(rows + [dict(rows[0], id=99, conclusion="failure")]) == 1
assert result(rows + [dict(rows[0], id=99, status="in_progress", conclusion=None)]) == 2
assert result(rows + [dict(rows[0], id=99, conclusion="cancelled")]) == 1
print("COMPILER_DELIVERY_SUCCESS_MISSING_STALE_FAILURE_PENDING_CANCELLED=PASS")
