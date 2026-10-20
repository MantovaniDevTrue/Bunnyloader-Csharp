from pathlib import Path

root = Path(__file__).resolve().parents[2]
native = (root / "app/src/main/cpp/experiments/csharp/NativeInvasionNetwork.cpp").read_text()
net = (root / "app/src/main/cpp/menu/NetRequests.cpp").read_text()
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()
wire = (root / "experiments/csharp/BunnyCsSdk/BunnyInvasionWire.cs").read_text()
bridge = (root / "experiments/csharp/BunnyCsSdk/BunnyInvasionNativeNetwork.cs").read_text()

for contract in (
    'sendNetworkChat(', 'g_peers.size() >= 32',
    'g_received.size() >= 64', 'g_hostStates.size() >= 64',
    'g_nonce', 'registerNativeInvasionClient(',
    'acceptNativeInvasionChat(', 'bunny_invasion_net_publish(',
    'bunny_invasion_net_poll(', 'runtime::sendToServer("invjoin "',
    'parseSnapshot(encoded, &id, &revision)',
):
    assert contract in native, contract
assert 'void onText(' in native
assert 'hook::install(method, onText, &g_originalText)' in native
assert 'token != g_nonce' in native
assert 'peer.delivered.clear()' in native
assert 'sendNetworkChat(' in net
assert 'r.command.rfind("invjoin ", 0)' in net
assert 'registerNativeInvasionClient(' in net
assert 'setNativeInvasionNetWorldState(invasionWorldReady, invasionWorldEpoch)' in auto
assert 'pumpNativeInvasionNetworkOnGameThread()' in auto
assert 'BunnyInvasionNativeNetwork.TryPoll(' in dispatcher
assert 'BunnyInvasionNativeNetwork.Publish(' in dispatcher
assert 'ReceiveRemoteFromHost(state, worldSession' in dispatcher
assert 'TryGetInvasionView(' in dispatcher
assert 'ReceiveRemoteFromHost(' in wire
assert 'remoteWorldSession != state.WorldSession' in wire
assert 'EntryPoint = "bunny_invasion_net_publish"' in bridge
assert 'EntryPoint = "bunny_invasion_net_poll"' in bridge
print("CSharp invasion host/client packet relay contract: OK")
