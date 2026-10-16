#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cpp = root / "app/src/main/cpp"
with tempfile.TemporaryDirectory(prefix="bunny-live-queue-") as temporary:
    binary = Path(temporary) / "queue"
    subprocess.run([
        "g++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(cpp),
        str(root / "tests/managed_live_queue_smoke.cpp"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True, timeout=15)
