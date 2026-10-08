package dev.bunnyloader.csharp

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.Process
import android.util.Log
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread

class ManagedProbeService : Service() {
    private val running = AtomicBoolean(false)

    private external fun runNative(assemblyPath: String, checkpointPath: String): String

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (!running.compareAndSet(false, true)) return START_NOT_STICKY
        val dir = File(filesDir, "bunny-csharp")
        val checkpoint = File(dir, "last-isolated-probe.txt")
        val status = File(dir, "isolated-worker-status.txt")
        val log = File(dir, "isolated-worker-logcat.txt")
        dir.mkdirs()
        checkpoint.delete()
        log.delete()
        runCatching { status.writeText("state=starting\npid=${Process.myPid()}\n") }
        startLogCapture(log)

        thread(name = "BunnyMonoDiagnostic", isDaemon = true) {
            try {
                System.loadLibrary("bunnymonoprobe")
                status.writeText("state=preparing-framework\npid=${Process.myPid()}\n")
                val assembly = installManagedFramework(dir)
                status.writeText("state=calling-mono\npid=${Process.myPid()}\n")
                val result = runNative(assembly, checkpoint.absolutePath)
                status.writeText("state=finished\npid=${Process.myPid()}\n$result\n")
            } catch (error: Throwable) {
                val detail = (error.javaClass.simpleName + ": " + (error.message ?: "")).take(512)
                runCatching { status.writeText("state=failed\npid=${Process.myPid()}\nerror=$detail\n") }
                Log.e("BunnyCSharp", "mono worker failed", error)
            } finally {
                running.set(false)
                stopSelf(startId)
            }
        }
        return START_NOT_STICKY
    }

    private fun installManagedFramework(dir: File): String {
        val version = dev.bunnyloader.BuildConfig.VERSION_NAME
        val marker = File(dir, "framework-version.txt")
        val names = assets.list("managed").orEmpty()
            .filter { it.endsWith(".dll") }
            .sorted()
        require(names.contains("System.Private.CoreLib.dll"))
        require(names.contains("System.Runtime.dll"))
        require(names.contains("BunnyManagedProbe.dll"))
        if (marker.isFile && marker.readText() == version &&
            names.all { File(dir, it).isFile && File(dir, it).length() > 0L }) {
            return File(dir, "BunnyManagedProbe.dll").absolutePath
        }
        marker.delete()
        for (name in names) {
            val target = File(dir, name)
            val temporary = File(dir, name + ".tmp")
            assets.open("managed/" + name).use { source ->
                temporary.outputStream().use { output -> source.copyTo(output) }
            }
            if (!temporary.renameTo(target)) {
                temporary.copyTo(target, overwrite = true)
                temporary.delete()
            }
        }
        marker.writeText(version)
        Log.i("BunnyCSharp", "framework carregado: ${names.size} assemblies")
        return File(dir, "BunnyManagedProbe.dll").absolutePath
    }

    private fun startLogCapture(destination: File) {
        thread(name = "BunnyMonoLog", isDaemon = true) {
            runCatching {
                val process = Runtime.getRuntime().exec(
                    arrayOf("logcat", "-v", "brief", "--pid=${Process.myPid()}")
                )
                process.inputStream.bufferedReader().useLines { lines ->
                    lines.forEach { line ->
                        if (destination.length() > 64000) return@forEach
                        if (line.contains("Mono", ignoreCase = true) ||
                            line.contains("libc") || line.contains("Fatal") ||
                            line.contains("FATAL") || line.contains("abort") ||
                            line.contains("AndroidRuntime") || line.contains("BunnyCSharp")) {
                            destination.appendText(line + "\n")
                        }
                    }
                }
            }
        }
    }
}
