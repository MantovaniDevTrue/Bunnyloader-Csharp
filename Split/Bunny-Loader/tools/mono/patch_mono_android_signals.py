#!/usr/bin/env python3
import sys
from pathlib import Path

source = Path(sys.argv[1])
text = source.read_text(encoding="utf-8")


def replace_once(before, after):
    global text
    count = text.count(before)
    if count != 1:
        raise SystemExit(f"Mono 8.0.23 patch mismatch: {count} occurrences")
    text = text.replace(before, after, 1)


replacements = [
    ("abort_signal_get", "SIGTTIN"),
    ("suspend_signal_get", "SIGPWR"),
    ("restart_signal_get", "SIGXCPU"),
]
for function, original_signal in replacements:
    prefix = f"static int\n{function} (void)\n{{\n#if defined(HOST_ANDROID)\n\treturn {original_signal};"
    adjusted = (
        f"static int\n{function} (void)\n{{\n"
        "#if defined(HOST_ANDROID) && defined(__aarch64__)\n"
        "\treturn mono_threads_suspend_search_alternative_signal ();\n"
        f"#elif defined(HOST_ANDROID)\n\treturn {original_signal};"
    )
    replace_once(prefix, adjusted)

replace_once(
    "#ifdef HOST_ANDROID\n"
    "\tg_assert (suspend_signal_num < 32);\n"
    "\tg_assert (restart_signal_num < 32);\n"
    "\tg_assert (abort_signal_num < 32);\n"
    "#endif",
    "#if defined(HOST_ANDROID) && !defined(__aarch64__)\n"
    "\tg_assert (suspend_signal_num < 32);\n"
    "\tg_assert (restart_signal_num < 32);\n"
    "\tg_assert (abort_signal_num < 32);\n"
    "#endif"
)

replace_once(
    "static int abort_signal_num = -1;\n",
    "static int abort_signal_num = -1;\n"
    "\n#if defined(HOST_ANDROID) && defined(__aarch64__)\n"
    "__attribute__((visibility(\"default\")))\n"
    "int mono_bunny_android_signal_policy_v1 (void)\n"
    "{\n"
    "\treturn 1;\n"
    "}\n"
    "#endif\n"
)

source.write_text(text, encoding="utf-8")
print("BUNNY_MONO_ARM64_RT_SIGNALS_PATCHED=YES")
