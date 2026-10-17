from pathlib import Path

root = Path(__file__).resolve().parents[1]
launcher = (root / "app/src/main/kotlin/dev/bunnyloader/LauncherActivity.kt").read_text()
launch = (root / "app/src/main/kotlin/dev/bunnyloader/csharp/LaunchSourcePrecompile.kt").read_text()
worker = (root / "app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt").read_text()
jni = (root / "app/src/main/cpp/experiments/csharp/MonoWorkerJni.cpp").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
source = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()

assert launcher.count("LaunchSourcePrecompile.begin(ctx, csharpLaunchSpecs)") == 2
assert 'fun begin(context: Context, selectedSpecs: List<String>)' in launch
assert 'for (spec in selectedSpecs.take(16))' in launch
assert 'source-launch-startup.txt' in launch
assert 'thread(name = "BunnyCSharpLaunchPrecompile"' not in launch
assert 'LaunchSourcePrecompile.begin(ctx, csharpLaunchSpecs)' in launcher
assert 'sourceLaunchSpecs' in launch
assert 'entry.endsWith(".cs")' in launch
assert "ModManifest.isValidUid(uid)" in launch
assert 'source.path.startsWith(content.path + File.separator)' in launch
assert 'putExtra("sourceLaunchSpecs",' in launch
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

receipt = (root / "app/src/main/kotlin/dev/bunnyloader/csharp/CSharpPrefetchReceipt.kt").read_text()
assert 'CSharpPrefetchReceipt' in worker
assert 'CSharpPrefetchReceipt.save(' in worker
assert 'CSharpPrefetchReceipt.ready(' in worker
assert 'verifyPrefetchedNative(' in worker
assert 'prefetchReused=' in worker
assert '"framework-version.txt"' in receipt
assert '"BUNNY_PREFETCH_RECEIPT_V1"' in receipt
assert 'sourceStamp(root, dir, id)' in receipt
assert 'hash(assembly)' in receipt
assert 'hash(File(assembly.absolutePath + ".sha256"))' in receipt
assert 'if (precompileOnly) {' in runtime
assert 'if (!verifyOnly) {' in runtime
assert 'out.result = 2;' in runtime
assert 'Java_dev_bunnyloader_csharp_ManagedProbeService_verifyPrefetchedNative' in jni
assert 'bool precompileOnly, bool verifyOnly' in jni
print("CSHARP_PREFETCH_RECEIPT_VERIFIED_REUSE_PASS")

begin = launch.split("fun begin(", 1)[1].split("internal fun schedule(", 1)[0]
assert "File(" not in begin and "ModRepository(" not in begin and ".assets.open(" not in begin
assert "app.startService(intent)" in begin
assert 'LaunchSourcePrecompile.schedule(this, job.launchSpecs' in worker
assert 'pending.poll(8, TimeUnit.SECONDS)' in worker
assert 'if (job.launchSpecs == null) residency.compiled++' in worker
assert 'queueWaitMs=' in launch and 'scanThread=' in launch
print("CSHARP_PLAY_DISPATCH_NO_PACKAGE_IO_WORKER_SCAN_PASS")

compiler = (root / "experiments/csharp/BunnySourceCompiler/Compiler.cs").read_text()
for contract in ("MaxFiles = 4096", "MaxFileBytes = 1024 * 1024", "MaxTotalBytes = 32 * 1024 * 1024"):
    assert contract in compiler
assert "sources.size > 4096" in receipt
assert "1L..1048576L" in receipt and "32L * 1024 * 1024" in receipt
assert ".take(4097)" in receipt
print("CSHARP_PREFETCH_LIMITS_MATCH_COMPILER_PASS")
