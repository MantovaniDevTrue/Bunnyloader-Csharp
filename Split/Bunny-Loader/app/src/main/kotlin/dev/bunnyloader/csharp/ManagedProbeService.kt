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
                status.writeText("state=calling-mono\npid=${Process.myPid()}\n")
                val assembly = File(dir, "BunnyManagedProbe.dll").absolutePath
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
