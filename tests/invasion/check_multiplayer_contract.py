from pathlib import Path

root = Path(__file__).resolve().parents[2]
net = (root / "app/src/main/cpp/menu/NetRequests.cpp").read_text()
items = (root / "app/src/main/cpp/experiments/csharp/NativeCSharpItemUse.cpp").read_text()
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
managed = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()
wire = (root / "experiments/csharp/BunnyCsSdk/BunnyInvasionWire.cs").read_text()

assert 'r.command.rfind("invuse ", 0)' in net
assert '"invuse %128s %128s %1s"' in net
assert 'std::chrono::milliseconds(300)' in net
assert 'enqueueVerifiedRemoteItemUse(r.client, mod, itemName)' in net
assert 'runtime::sendToServer("invuse " + binding->second.mod' in items
assert 'bool enqueueVerifiedRemoteItemUse(' in items
for name in ['"selectedItem"', '"inventory"', '"active"']:
    assert name in items, name
assert 'runtime::modItemTypeByKey(mod + "/" + itemName)' in items
assert 'runtime::field<int32_t>(selectedItem, g_itemTypeOffset) != type' in items
assert 'if (mode == 1)' in dispatcher
assert 'BunnyNativeItemUse.Bind(worldSession, context.Mod, itemName)' in dispatcher
assert 'if (mode == 2) return true;' in auto
assert 'context.netMode >= 0 && context.netMode <= 3' in auto
assert 'else if ((mode == 0 || mode == 2) && runtime::resolveGameRefs())' in auto
assert 'if (snapshot.ready && snapshot.player.life > 0' in auto
assert 'runtime::field<uint8_t>(player, activeOffset) == 0' in auto
assert 'runtime::field<uint8_t>(player, activeOffset) == 0' in auto
assert '(mode < 0 || mode > 3)' in managed
assert 'public sealed class BunnyInvasionClientView' in wire
assert 'state.WorldSession != currentWorldSession' in wire
assert 'state.Epoch < epoch' in wire
assert 'state.Revision <= revision' in wire
print("Native CSharp invasion multiplayer request and model contract: OK")
