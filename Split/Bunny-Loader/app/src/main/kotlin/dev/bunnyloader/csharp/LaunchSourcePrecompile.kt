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
        if (!scheduling.compareAndSet(false, true)) return
        val app = context.applicationContext
        val started = android.os.SystemClock.elapsedRealtime()
        var queued = 0
        var skipped = 0
        try {
            val modsDir = ModRepository(app).modsDir
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
                if (assembly.isFile && assembly.length() > 0 && assemblyCache.isFile &&
                    assemblyCache.length() in 1L..256L &&
                    content.walkTopDown().filter { it.isFile && it.extension == "cs" }
                        .none { it.lastModified() > assembly.lastModified() }) {
                    skipped++
                    Log.i("BunnyCSharp", "[CSharp][Startup] Stage=LaunchPrecompileSkipExisting Mod=$uid")
                    continue
                }
                val intent = Intent(app, ManagedProbeService::class.java)
                    .putExtra("sourceRoot", root.absolutePath)
                    .putExtra("sourceModId", uid)
                    .putExtra("sourcePrecompile", true)
                if (runCatching { app.startService(intent) }.getOrNull() != null) {
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
                    writeText("queued=$queued\nskipped=$skipped\nelapsedMs=$elapsed\n")
                }
            }
            scheduling.set(false)
        }
    }
}
