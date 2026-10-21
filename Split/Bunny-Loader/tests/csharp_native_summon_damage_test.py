"""Keep the historical CI entrypoint, execute production hooks instead of lexical assertions."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
subprocess.run(["bash", "tests/summon_lifecycle/run.sh"], cwd=root, check=True)
print("BUNNY_CSHARP_SUMMON_PRODUCTION_RUNTIME_AND_SOURCE_DAMAGE=PASS")
