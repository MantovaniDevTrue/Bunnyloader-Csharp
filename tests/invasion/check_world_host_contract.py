from pathlib import Path

root = Path(__file__).resolve().parents[2]
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
managed = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()
mailbox = (root / "app/src/main/cpp/experiments/csharp/NativeInvasionMailbox.cpp").read_text()

start = auto.index("bool worldPlayerReady()")
world_ready = auto[start:start + 3200]
assert "if (mode < 0 || mode > 3) return false;" in world_ready
assert "if (mode != 0) return false;" not in world_ready

assert "pumpNativeInvasionSpawnsOnGameThread();" in auto
assert "setNativeInvasionWorldState(false, g_worldEpoch.load(" in auto
assert "scheduleManagedInvasionWorldTicks(worldSession, mode," in auto
assert "if (++invasionFrames % 30 == 0" in auto
assert "captureTerrariaSnapshot(false, false)" in auto
assert "g_entityWorldTick = g_api.findMethod(klass, \"TickWorld\", 7);" in managed
assert "job.kind = 6;" in managed
assert "g_entityQueue.size() >= 16" in managed
assert "invasionSupported = false;" in managed
assert "static int TickWorld(int modSlot, int mode, int elapsedFrames," in dispatcher
assert "BunnyInvasionNativeTransport.Submit(" in dispatcher
assert "BunnyInvasionNativeTransport.TryPoll(" in dispatcher
assert "bunny_invasion_submit" in mailbox and "bunny_invasion_poll" in mailbox
assert "g_requests.size() >= kMaxPending" in mailbox
assert "g_session.load(std::memory_order_acquire) !=" in mailbox
print("World host/native mailbox/MonoVM tick contracts: OK")
