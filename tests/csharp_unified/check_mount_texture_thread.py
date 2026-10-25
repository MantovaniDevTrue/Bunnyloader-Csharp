from pathlib import Path

root = Path(__file__).resolve().parents[2]
mount = (root / "app/src/main/cpp/experiments/csharp/NativeMountRegistry.cpp").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
header = (root / "app/src/main/cpp/experiments/csharp/NativeMountRegistry.h").read_text()

assert "bool publishNativeMountsImmediate(" in mount
assert "bool publishNativeMounts(" in mount
assert "void pumpNativeMountPublicationsOnGameThread()" in mount
assert "void pumpNativeMountPublicationsOnGameThread();" in header
assert "    pumpNativeMountPublicationsOnGameThread();" in runtime
assert "publishQueue.push_back(job)" in mount
assert "publishQueue.pop_front()" in mount
assert "job->condition.wait_for(lock,std::chrono::seconds(30)" in mount
assert "job->cancelled=true;" in mount
assert "const bool success=publishNativeMountsImmediate(" in mount

immediate = mount.split("bool publishNativeMountsImmediate(", 1)[1].split("bool publishNativeMounts(", 1)[0]
queued = mount.split("bool publishNativeMounts(", 1)[1].split("void pumpNativeMountPublicationsOnGameThread()", 1)[0]
assert immediate.count("runtime::content::loadTextureAsset(") == 3
assert "runtime::content::loadTextureAsset(" not in queued
assert "publishNativeMountsImmediate(mod,directory,entries,error)" in queued
assert "publishNativeMounts(mod.id" not in runtime

print("CSHARP_MOUNT_TEXTURE_GAME_THREAD_PASS")
