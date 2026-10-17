package dev.bunnyloader.csharp

import android.content.Context
import android.content.Intent
import android.util.Log
import dev.bunnyloader.mods.ModManifest
import dev.bunnyloader.mods.ModRepository
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

object LaunchSourcePrecompile {
    private val scheduling = AtomicBoolean(false)

    fun begin(context: Context, selectedSpecs: List<String>) {
        if (selectedSpecs.isEmpty() || !scheduling.compareAndSet(false, true)) return
        val app = context.applicationContext
        try {
            // Start while the launcher is foreground; inspect packages only on the service worker.
            val intent = Intent(app, ManagedProbeService::class.java)
                .putExtra("sourceLaunchSpecs", selectedSpecs.take(16).distinct().toTypedArray())
                .putExtra("sourceLaunchStartedMs", android.os.SystemClock.elapsedRealtime())
            app.startService(intent)
        } catch (error: Exception) {
            Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Stage=LaunchDispatch Reason=ServiceStartFailed Result=OfficialCompileFallback", error)
        } finally {
            scheduling.set(false)
        }
    }

    internal fun schedule(context: Context, selectedSpecs: List<String>, launchStartedMs: Long,
                          enqueue: (String, String) -> Boolean) {
        val app = context.applicationContext
        val started = android.os.SystemClock.elapsedRealtime()
        var queued = 0
        var skipped = 0
        try {
            val modsDir = ModRepository(app).modsDir
            val expectedFramework = runCatching {
                val manifest = app.assets.open("managed/framework.sha256").bufferedReader().use {
                    it.readText()
                }
                java.security.MessageDigest.getInstance("SHA-256")
                    .digest(manifest.toByteArray(Charsets.UTF_8))
                    .joinToString("") { "%02x".format(it.toInt() and 0xff) }
            }.getOrDefault("")
            val installedFramework = runCatching {
                File(app.filesDir, "bunny-csharp/framework-version.txt").readText().trim()
            }.getOrDefault("")
            val frameworkUnchanged = expectedFramework.isNotEmpty() &&
                (expectedFramework == installedFramework ||
                    installedFramework.endsWith(":" + expectedFramework))
            for (spec in selectedSpecs.take(16)) {
                val uid = spec.substringBefore('=')
                val entry = spec.substringAfter('=', "")
                if (!ModManifest.isValidUid(uid) || !entry.endsWith(".cs") ||
                    entry.startsWith('/') || entry.contains("..")) continue
                val root = File(modsDir, uid).canonicalFile
                val content = File(root, "content").canonicalFile
                val source = File(content, entry).canonicalFile
                if (!source.isFile || !source.path.startsWith(content.path + File.separator)) continue
                val assembly = File(app.filesDir, "bunny-csharp/compiled/$uid.dll")
                val assemblyCache = File(assembly.absolutePath + ".sha256")
                val nativeCachePresent = File(app.filesDir, "bunny-csharp/auto-source-cache")
                    .listFiles { candidate ->
                        candidate.isFile && candidate.name.startsWith("$uid-") &&
                            candidate.name.endsWith(".cache")
                    }.orEmpty().isNotEmpty()
                if (frameworkUnchanged && nativeCachePresent &&
                    assembly.isFile && assembly.length() > 0 && assemblyCache.isFile &&
                    assemblyCache.length() in 1L..256L &&
                    content.walkTopDown().filter { it.isFile && it.extension == "cs" }
                        .none { it.lastModified() > assembly.lastModified() }) {
                    skipped++
                    Log.i("BunnyCSharp", "[CSharp][Startup] Stage=LaunchPrecompileSkipExisting Mod=$uid")
                    continue
                }
                if (enqueue(root.absolutePath, uid)) {
                    queued++
                    Log.i("BunnyCSharp", "[CSharp][Startup] Stage=LaunchPrecompileQueued Mod=$uid")
                }
            }
            Log.i("BunnyCSharp", "[CSharp][Startup] Stage=LaunchPrecompileScheduled Mods=$queued Skipped=$skipped")
        } catch (error: Exception) {
            Log.w("BunnyCSharp", "Launch CSharp precompile unavailable", error)
        } finally {
            val elapsed = android.os.SystemClock.elapsedRealtime() - started
            runCatching {
                File(app.filesDir, "bunny-csharp/source-launch-startup.txt").apply {
                    parentFile?.mkdirs()
                    val queueWait = (started - launchStartedMs).coerceAtLeast(0)
                    writeText("queued=$queued\nskipped=$skipped\nelapsedMs=$elapsed\n" +
                        "queueWaitMs=$queueWait\nscanThread=${Thread.currentThread().name}\n")
                }
            }
        }
    }
}
