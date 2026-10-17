package dev.bunnyloader.csharp

import android.content.Context
import android.content.Intent
import android.util.Log
import dev.bunnyloader.mods.ModManifest
import dev.bunnyloader.mods.ModRepository
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread

object LaunchSourcePrecompile {
    private val scheduling = AtomicBoolean(false)

    fun begin(context: Context) {
        if (!scheduling.compareAndSet(false, true)) return
        val app = context.applicationContext
        thread(name = "BunnyCSharpLaunchPrecompile", isDaemon = true) {
            try {
                val repository = ModRepository(app)
                val enabled = repository.enabledSpecs()
                var queued = 0
                for (spec in enabled) {
                    val uid = spec.substringBefore('=')
                    val entry = spec.substringAfter('=', "")
                    if (!ModManifest.isValidUid(uid) || !entry.endsWith(".cs") ||
                        entry.startsWith('/') || entry.contains("..")) continue
                    val root = File(repository.modsDir, uid).canonicalFile
                    val content = File(root, "content").canonicalFile
                    val source = File(content, entry).canonicalFile
                    if (!source.isFile || !source.path.startsWith(content.path + File.separator)) continue
                    val assembly = File(app.filesDir, "bunny-csharp/compiled/$uid.dll")
                    val assemblyCache = File(assembly.absolutePath + ".sha256")
                    if (assembly.isFile && assembly.length() > 0 && assemblyCache.isFile &&
                        assemblyCache.length() in 1..256 &&
                        content.walkTopDown().filter { it.isFile && it.extension == "cs" }
                            .none { it.lastModified() > assembly.lastModified() }) {
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
                Log.i("BunnyCSharp", "[CSharp][Startup] Stage=LaunchPrecompileScheduled Mods=$queued")
            } catch (error: Exception) {
                Log.w("BunnyCSharp", "Launch CSharp precompile unavailable", error)
            } finally {
                scheduling.set(false)
            }
        }
    }
}
