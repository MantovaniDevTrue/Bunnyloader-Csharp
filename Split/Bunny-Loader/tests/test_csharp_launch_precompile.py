from pathlib import Path

root = Path(__file__).resolve().parents[1]
launcher = (root / "app/src/main/kotlin/dev/bunnyloader/LauncherActivity.kt").read_text()
launch = (root / "app/src/main/kotlin/dev/bunnyloader/csharp/LaunchSourcePrecompile.kt").read_text()
worker = (root / "app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt").read_text()
jni = (root / "app/src/main/cpp/experiments/csharp/MonoWorkerJni.cpp").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
source = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()

assert launcher.count("LaunchSourcePrecompile.begin(ctx)") == 2
assert launch.index('thread(name = "BunnyCSharpLaunchPrecompile"') < launch.index('repository.enabledSpecs()')
assert 'entry.endsWith(".cs")' in launch
assert "ModManifest.isValidUid(uid)" in launch
assert 'source.path.startsWith(content.path + File.separator)' in launch
assert 'putExtra("sourcePrecompile", true)' in launch
assert 'app.startService(intent)' in launch

assert 'LinkedBlockingDeque<CompileWork>(16)' in worker
assert 'if (precompile) pending.offerLast(work) else pending.offerFirst(work)' in worker
assert 'val displaced = pending.peekLast()' in worker
assert '"source-launch-precompile-" + job.modId + ".txt"' in worker
assert 'if (!job.precompile)' in worker
assert 'File(diagnostic.absolutePath + ".profile")' in worker
assert 'if (job.precompile) precompileNative(' in worker
assert '"phase=compile-prefetched"' in worker
assert '"precompileOnly="' in worker

assert "Java_dev_bunnyloader_csharp_ManagedProbeService_precompileNative" in jni
assert "Java_dev_bunnyloader_csharp_ManagedProbeService_compileNative" in jni
assert "precompileOnly" in jni
assert 'if (precompileOnly) {' in runtime
assert 'out.phase = "compile-prefetched";' in runtime
assert runtime.index('if (precompileOnly) {') < runtime.index('trace("compiled-assembly-execution-start");')
assert 'const auto verifyStarted = std::chrono::steady_clock::now();' in runtime

assert source.index('Stage=EarlyCompile') < source.index('note(&status, "waiting-content");')
assert 'readCache(cacheFile, mod.id, key, assembly' in source
assert 'applyManagedSourceDefaults(mod.id, defaults, &error)' in source

print("CSHARP_PLAY_CLICK_PRECOMPILE_AND_CONTENT_GATE_PASS")
