from pathlib import Path

root = Path(__file__).resolve().parents[2]
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
host = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()

assert 'mod.npcs > 0 || mod.projectiles > 0 || mod.items > 0' in auto
assert '(npcTypes.empty() && projectileTypes.empty())) return false;' not in host
assert 'registerManagedEntitySourceMod(mod.id, assembly, npcs, projectiles)' in auto
assert 'typeof(BunnyItem).IsAssignableFrom(type)' in dispatcher
assert 'typeof(BunnyItem.OnUse)' not in dispatcher
assert 'context.Invasions.Count == 0 && context.ItemUsers.Count == 0' in dispatcher
assert 'context.ItemUsers.TryAdd(type.Name, item)' in dispatcher
assert 'BunnyNativeItemUse.Bind(worldSession, context.Mod, itemName)' in dispatcher
print("CSharp BunnyItem-only discovery and managed item-use hook contract: OK")
