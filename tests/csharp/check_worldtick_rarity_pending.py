from pathlib import Path

root = Path(__file__).resolve().parents[2]
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()
native = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
rarity = (root / "app/src/main/cpp/experiments/csharp/NativeRarityGameplay.cpp").read_text()

if "context.Invasions.Count == 0 && context.ItemUsers.Count == 0 && context.WorldSystems.Count == 0) return -2;" not in " ".join(sdk.split()):
    raise SystemExit("WorldTick sentinel changed upstream")
for marker in ("worldTickFailed(count)", "worldTickUnsupported(result)",
               "Stage=FirstWorldTick Result=Skipped Reason=NoInvasionsOrItemUsers"):
    if marker not in native:
        raise SystemExit("WorldTick sentinel misreported: " + marker)
for marker in ("rarityRetryPending(response)", "rarityRetryAt(g_tick)",
               "raritySampleReady(g_tick, mod.nextSampleTick)"):
    if marker not in rarity:
        raise SystemExit("Missing bounded retry for pending rarity callback: " + marker)
print("WORLD_TICK_AND_RARITY_PENDING_PARITY_PASS")
