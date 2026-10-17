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

    fun begin(context: Context, selectedSpecs: List<String>, origin: String = "play") {
        if (!scheduling.compareAndSet(false, true)) return
        val app = context.applicationContext
        val started = android.os.SystemClock.elapsedRealtime()
        val auto = origin == "launcher"
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
                val receipt = File(assembly.parentFile, "$uid.launch-ready")
                val prefetchPresent = receipt.isFile && receipt.length() in 64L..512L &&
                    receipt.lastModified() >= assembly.lastModified()
                val compiledCurrent = assembly.isFile && assembly.length() > 0 &&
                    assemblyCache.isFile && assemblyCache.length() in 1L..256L &&
                    content.walkTopDown().filter { it.isFile && it.extension == "cs" }
                        .none { it.lastModified() > assembly.lastModified() }
                if (frameworkUnchanged && compiledCurrent &&
                    (nativeCachePresent || prefetchPresent)) {
                    skipped++
                    Log.i("BunnyCSharp", "[CSharp][Startup] Stage=${if (auto) "LauncherAutoSkipCached" else "LaunchPrecompileSkipExisting"} Mod=$uid PrefetchPresent=${if (prefetchPresent) 1 else 0}")
                    continue
                }
                val intent = Intent(app, ManagedProbeService::class.java)
                    .putExtra("sourceRoot", root.absolutePath)
                    .putExtra("sourceModId", uid)
                    .putExtra("sourcePrecompile", true)
                if (runCatching { app.startService(intent) }.getOrNull() != null) {
                    queued++
                    Log.i("BunnyCSharp", "[CSharp][Startup] Stage=${if (auto) "LauncherAutoQueued" else "LaunchPrecompileQueued"} Mod=$uid")
                }
            }
            Log.i("BunnyCSharp", "[CSharp][Startup] Stage=${if (auto) "LauncherAutoScheduled" else "LaunchPrecompileScheduled"} Mods=$queued Skipped=$skipped")
        } catch (error: Exception) {
            Log.w("BunnyCSharp", "Launch CSharp precompile unavailable", error)
        } finally {
            val elapsed = android.os.SystemClock.elapsedRealtime() - started
            runCatching {
                File(app.filesDir, if (auto) "bunny-csharp/source-launch-auto.txt" else
                    "bunny-csharp/source-launch-startup.txt").apply {
                    parentFile?.mkdirs()
                    writeText("origin=$origin\nqueued=$queued\nskipped=$skipped\nelapsedMs=$elapsed\n")
                }
            }
            scheduling.set(false)
        }
    }
}
