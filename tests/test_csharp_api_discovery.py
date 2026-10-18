from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
script = ROOT / "tools/csharp_api_discovery.py"
spec = importlib.util.spec_from_file_location("bunny_api", script)
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)

def test_sources():
    report = api.build_catalog()
    assert report["summary"]["jsFiles"] >= 100
    assert report["summary"]["sdkClasses"] >= 7
    assert report["summary"]["features"] >= 16

def test_js_does_not_imply_csharp():
    report = api.build_catalog()
    assert "SetBestiary" in report["javascriptGroups"]["ModNPC"]["methods"]
    by_id = {x["id"]: x for x in report["features"]}
    assert by_id["npc_bestiary"]["status"] == "not_implemented"
    assert not by_id["npc_bestiary"].get("example")

def test_android_evidence():
    report = api.build_catalog()
    by_id = {x["id"]: x for x in report["features"]}
    for key in ("managed_npc_impulse", "managed_npc_debuff", "on_hit_context_v4"):
        assert by_id[key]["status"] == "device_verified"
        assert by_id[key]["probes"]

if __name__ == "__main__":
    test_sources()
    test_js_does_not_imply_csharp()
    test_android_evidence()
    print("BUNNY_CSHARP_API_DISCOVERY_TEST=PASS")
