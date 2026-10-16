import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
script = ROOT / "tools/csharp_api_gap_map.py"
spec = importlib.util.spec_from_file_location("api_gaps", script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def test_real_js_references_only():
    report = module.produce()
    assert report["summary"]["total"] >= 10
    assert all(x["referenceCount"] > 0 for x in report["candidates"])
    assert all(x["implementedInCSharp"] is False for x in report["candidates"])
    assert all(x["state"] == "research_only" for x in report["candidates"])

def test_prioritized_developer_gaps():
    entries = {x["id"]: x for x in module.produce()["candidates"]}
    assert entries["item_prefixes"]["priority"] == 1
    assert "ModifyNPCLoot" in entries["npc_loot"]["callbacks"]
    assert "SaveWorldData" in entries["world_save"]["callbacks"]
    assert entries["projectile_spawning"]["referenceCount"] >= 1

if __name__ == "__main__":
    test_real_js_references_only()
    test_prioritized_developer_gaps()
    print("BUNNY_CSHARP_API_GAP_PLAN=PASS")
