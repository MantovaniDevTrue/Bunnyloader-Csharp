"""Exercise the production wire parser against three actual compiled example items."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
names = {"ExamplePickaxe", "ExampleHamaxe", "ExampleItem"}
rows = [row for row in Path(sys.argv[1]).read_text().splitlines()
        if row.startswith("ITEM\t") and row.split("\t")[1] in names]
assert len(rows) == len(names), "Missing compiled example wire"
with tempfile.TemporaryDirectory(prefix="bunny-basic-wire-") as folder:
    source = Path(folder) / "items.txt"
    source.write_text("BUNNY_MANAGED_DEFAULTS_V2\n" + "\n".join(rows) + "\n")
    executable = Path(folder) / "wire"
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(root / "app/src/main/cpp"),
                    str(root / "tests/basic_items/basic_wire_test.cpp"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable), str(source)], check=True, timeout=30)
