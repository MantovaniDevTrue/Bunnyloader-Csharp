from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
p = ROOT / "tools/csharp_api_negative_compile.py"
spec = importlib.util.spec_from_file_location("negative_api", p)
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)

def test_expected_api_rejections():
    ids = {x["id"] for x in api.CASES}
    assert len(api.CASES) >= 5
    assert len(ids) == len(api.CASES)
    assert all(x["error"].startswith("CS") for x in api.CASES)
    assert all("using BunnyLoader.Source;" in x["source"] for x in api.CASES)

if __name__ == "__main__":
    test_expected_api_rejections()
    print("BUNNY_CSHARP_NEGATIVE_API_FIXTURES=PASS")
