#!/usr/bin/env python3
import re
import sys
from pathlib import Path

content = Path(sys.argv[1]).read_text(encoding="utf-8")
for function in ("abort_signal_get", "suspend_signal_get", "restart_signal_get"):
    pattern = (
        rf"static int\n{function} \(void\)\n"
        r"\{\n#if defined\(HOST_ANDROID\) && defined\(__aarch64__\)\n"
        r"\treturn mono_threads_suspend_search_alternative_signal \(\);"
    )
    assert re.search(pattern, content), function
assert "int mono_bunny_android_signal_policy_v1 (void)" in content
assert "#if defined(HOST_ANDROID) && !defined(__aarch64__)" in content
print("MONO_ARM64_SIGNAL_SOURCE_CHECK=PASS")
