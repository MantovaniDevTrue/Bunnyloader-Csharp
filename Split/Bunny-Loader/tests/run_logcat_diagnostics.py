"""Compile and run the production report classifier without Android dependencies."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="bunny-logcat-") as temporary:
    javac = shutil.which("javac")
    compiler = [javac] if javac else ["java", "com.sun.tools.javac.Main"]
    subprocess.run(compiler + ["-d", temporary,
        str(root / "app/src/main/java/dev/bunnyloader/game/LogcatDiagnostics.java"),
        str(root / "tests/LogcatDiagnosticsTest.java")], check=True)
    subprocess.run(["java", "-cp", temporary, "LogcatDiagnosticsTest"], check=True)
