from pathlib import Path

root = Path(__file__).resolve().parents[2]
native = (root / "app/src/main/cpp/experiments/csharp/NativeInvasionPersistence.cpp").read_text()
path = (root / "app/src/main/cpp/experiments/csharp/NativeInvasionWorldPath.cpp").read_text()
store = (root / "app/src/main/cpp/experiments/csharp/InvasionSaveFile.h").read_text()
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnyInvasionWorldPersistence.cs").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()

assert "Main" in path and "ActiveWorldFileData" in path
assert '"_isCloudSave"' in path and '"_path"' in path
assert 'setNativeInvasionSaveWorld(false, session, {}, false);' in path
assert '"bunny-invasions 1"' in store
assert 'std::rename(temp.c_str(), path.c_str())' in store
assert 'fsync(fileno(file))' in store
assert 'readFile(path, &records)' in store
assert 'validName(mod, 128)' in store and 'validName(id, 64)' in store
assert '"bunny_invasion_state_read"' in sdk
assert '"bunny_invasion_state_write"' in sdk
assert 'bunny_invasion_state_read(' in native
assert 'bunny_invasion_state_write(' in native
assert 'if (!active(session)) return 0;' in native
assert '!cloud' in native
assert 'path.substr(path.size() - 4) == ".wld"' in native
assert 'updateNativeInvasionWorldPath(true, invasionWorldEpoch)' in auto
assert 'updateNativeInvasionWorldPath(false, g_worldEpoch.load(' in auto
assert 'BunnyInvasionWorldPersistence.TryRestore(' in dispatcher
assert 'SaveInvasionIfChanged(context);' in dispatcher
assert 'BunnyInvasionWorldPersistence.Save(' in dispatcher
assert 'context.SavedInvasions.TryGetValue(system.Id' in dispatcher
assert 'state.Progress != saved.Progress' in dispatcher
print("Native CSharp per-world invasion save contract: OK")
