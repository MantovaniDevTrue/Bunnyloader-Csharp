from pathlib import Path

root = Path(__file__).resolve().parents[2]
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()
host = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
sample = (root / "samples/ExampleModCSharp/Playable/content/Content/NPCs/ExampleSlimeNPC.cs").read_text()
pipeline = (root / "samples/ExampleModCSharp/Playable/content/Common/Systems/ExampleInvasion.cs").read_text()

assert "public long NativeIdentity;" in sdk
assert "float playerX, float playerY, long nativeIdentity)" in dispatcher
assert "NativeIdentity = nativeIdentity" in dispatcher
assert 'g_api.findMethod(klass, "TickNpc", 13)' in host
assert "int64_t nativeIdentity = static_cast<int64_t>(job.identity);" in host
assert "&nativeIdentity};" in host
assert "invasion.Pipeline.MatchesPendingNpc(" in dispatcher
assert "invasion.Pipeline.ConfirmFirstTick(true," in dispatcher
assert "invasion.Pipeline.Controller.ReportKill(true," in dispatcher
assert "invasion.Pipeline.Controller.ReportDespawn(true," in dispatcher
assert "ExampleInvasion.ConfirmFirstTick" not in sample
assert "Pipeline.ConfirmFirstTick(" in pipeline
assert "Pipeline.ReportCreated(" in pipeline
assert "Controller.CommitSpawn(" not in pipeline
print("Invasion native identity bridge: OK")
