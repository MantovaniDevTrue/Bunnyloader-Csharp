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
    companion object {

        @JvmStatic
        fun historicalWorkerTrace(context: android.content.Context, pid: Int, minimumTimestampMs: Long): String {
            if (android.os.Build.VERSION.SDK_INT < 30 || pid <= 0) return "trace-api-unavailable"
            return try {
                val manager = context.getSystemService(android.content.Context.ACTIVITY_SERVICE)
                    as? android.app.ActivityManager ?: return "activity-manager-unavailable"
                val name = context.packageName + ":mono_probe"
                val record = manager.getHistoricalProcessExitReasons(context.packageName, pid, 10)
                    .filter { it.pid == pid && it.processName == name &&
                        it.timestamp >= minimumTimestampMs }
                    .maxByOrNull { it.timestamp }
                    ?: return "exit-record-unavailable"
                val stream = record.traceInputStream
                    ?: return "trace-unavailable reason=" + record.reason +
                        " status=" + record.status
                val output = java.io.ByteArrayOutputStream()
                stream.use { source ->
                    val buffer = ByteArray(4096)
                    while (output.size() < 65536) {
                        val count = source.read(buffer, 0,
                            minOf(buffer.size, 65536 - output.size()))
                        if (count <= 0) break
                        output.write(buffer, 0, count)
                    }
                }
                val raw = output.toString("UTF-8").replace('\u0000', ' ')
                val interesting = raw.lineSequence().filterIndexed { index, line ->
                    index < 16 ||
                        line.contains("signal ", ignoreCase = true) ||
                        line.contains("Abort message", ignoreCase = true) ||
                        line.contains("backtrace", ignoreCase = true) ||
                        line.contains("Cause:", ignoreCase = true) ||
                        line.contains("libmonosgen", ignoreCase = true) ||
                        line.contains("libc.so", ignoreCase = true) ||
                        line.contains("BunnySource", ignoreCase = true) ||
                        line.contains("mono_", ignoreCase = true) ||
                        Regex("^\\s*#\\d+\\s+pc\\s+").containsMatchIn(line)
                }.take(150).joinToString("\n").take(16384)
                "traceBytes=" + output.size() + "\n" +
                    (interesting.ifEmpty { raw.take(12288) })
            } catch (error: Throwable) {
                "trace-query-error=" + error.javaClass.simpleName + ":" +
                    (error.message ?: "").take(180)
            }
        }

        @JvmStatic
        fun historicalWorkerExit(context: android.content.Context, pid: Int, minimumTimestampMs: Long): String {
            if (android.os.Build.VERSION.SDK_INT < 30 || pid <= 0) return "unavailable"
            return try {
                val activityManager = context.getSystemService(android.content.Context.ACTIVITY_SERVICE)
                    as? android.app.ActivityManager ?: return "activity-manager-unavailable"
                val processName = context.packageName + ":mono_probe"
                val record = activityManager.getHistoricalProcessExitReasons(context.packageName, pid, 10)
                    .filter { it.pid == pid && it.processName == processName &&
                        it.timestamp >= minimumTimestampMs }
                    .maxByOrNull { it.timestamp }
                    ?: return "exit-record-unavailable"
                val reason = when (record.reason) {
                    android.app.ApplicationExitInfo.REASON_CRASH_NATIVE -> "CRASH_NATIVE"
                    android.app.ApplicationExitInfo.REASON_CRASH -> "CRASH"
                    android.app.ApplicationExitInfo.REASON_LOW_MEMORY -> "LOW_MEMORY"
                    android.app.ApplicationExitInfo.REASON_ANR -> "ANR"
                    android.app.ApplicationExitInfo.REASON_SIGNALED -> "SIGNALED"
                    android.app.ApplicationExitInfo.REASON_USER_REQUESTED -> "USER_REQUESTED"
                    else -> "OTHER_" + record.reason
                }
                ("pid=" + record.pid + "\nreason=" + reason +
                    "\nstatus=" + record.status +
                    "\ntimestampMs=" + record.timestamp +
                    "\npssKb=" + record.pss + "\nrssKb=" + record.rss +
                    "\ndescription=" + (record.description ?: "").replace('\n', ' ')
                ).take(2048)
            } catch (error: Throwable) {
                "exit-lookup-error=" + error.javaClass.simpleName + ":" +
                    (error.message ?: "").take(180)
            }
        }
    }

    private val running = AtomicBoolean(false)

    private external fun runNative(assemblyPath: String, checkpointPath: String): String
    private external fun compileNative(
        probePath: String, compilerPath: String, sourceRoot: String,
        outputPath: String, diagnosticPath: String, checkpointPath: String
    ): String

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (!running.compareAndSet(false, true)) return START_NOT_STICKY
        val dir = File(filesDir, "bunny-csharp")
        val checkpoint = File(dir, "last-isolated-probe.txt")
        val status = File(dir, "isolated-worker-status.txt")
        val log = File(dir, "isolated-worker-logcat.txt")
        val compileRoot = intent?.getStringExtra("sourceRoot")
        val compileStatus = File(dir, "source-compile-status.txt")
        val diagnostic = File(dir, "source-compile-diagnostics.txt")
        val health = File(dir, "source-compile-health.txt")
        val nativeTrace = File(dir, "native-abort-trace.txt")
        val nativeMaps = File(dir, "native-abort-maps.txt")
        val nativeThreads = File(dir, "native-abort-threads.txt")
        dir.mkdirs()
        checkpoint.delete()
        log.delete()
        if (compileRoot == null) {
            runCatching { status.writeText("state=starting\npid=${Process.myPid()}\n") }
        } else {
            runCatching {
                diagnostic.delete()
                health.delete()
                nativeTrace.delete()
                nativeMaps.delete()
                nativeThreads.delete()
                compileStatus.writeText("state=preparing-framework\npid=${Process.myPid()}\n")
            }
        }
        startLogCapture(log)

        thread(name = "BunnyMonoDiagnostic", isDaemon = true) {
            try {
                System.loadLibrary("bunnymonoprobe")
                if (compileRoot == null)
                    status.writeText("state=preparing-framework\npid=${Process.myPid()}\n")
                val assembly = installManagedFramework(dir)
                if (compileRoot == null)
                    status.writeText("state=calling-mono\npid=${Process.myPid()}\n")
                if (compileRoot != null) {
                    val compiler = File(dir, "BunnySourceCompiler.dll")
                    val outputDir = File(dir, "compiled")
                    outputDir.mkdirs()
                    val output = File(outputDir, "sourcecsharp.dll")
                    compileStatus.writeText("state=compiling\npid=${Process.myPid()}\n")
                    startCompileHealthMonitor(health)
                    startNativeAbortSampler(nativeMaps, nativeThreads)
                    val result = compileNative(
                        assembly, compiler.absolutePath, compileRoot,
                        output.absolutePath, diagnostic.absolutePath, checkpoint.absolutePath
                    )
                    val passed = result.contains("phase=compile-verified") &&
                        (result.contains("result=1\n") || result.contains("result=2\n"))
                    compileStatus.writeText(
                        "state=${if (passed) "finished" else "failed"}\n" +
                            "pid=${Process.myPid()}\n$result\n" +
                            "assemblyReady=${if (passed && output.isFile) 1 else 0}\n"
                    )
                } else {
                    val result = runNative(assembly, checkpoint.absolutePath)
                    status.writeText("state=finished\npid=${Process.myPid()}\n$result\n")
                }
            } catch (error: Throwable) {
                val detail = (error.javaClass.simpleName + ": " + (error.message ?: "")).take(512)
                runCatching {
                    val target = if (compileRoot == null) status else compileStatus
                    target.writeText("state=failed\npid=${Process.myPid()}\nerror=$detail\n")
                }
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
        require(names.contains("BunnySourceCompiler.dll"))
        require(names.contains("BunnySourceSdk.dll"))
        require(names.contains("Microsoft.CodeAnalysis.dll"))
        require(names.contains("Microsoft.CodeAnalysis.CSharp.dll"))
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

    private fun startNativeAbortSampler(maps: File, threads: File) {
        thread(name = "BunnyMonoNativeSnapshot", isDaemon = true) {
            repeat(15) {
                if (!running.get()) return@thread
                runCatching {
                    val codeMappings = File("/proc/self/maps").useLines { entries ->
                        entries.filter { line ->
                            line.contains("r-xp") &&
                                (line.contains("libmonosgen") ||
                                    line.contains("libc.so") ||
                                    line.contains("libbunnymonoprobe") ||
                                    line.contains("libSystem.") ||
                                    line.contains("libart.so") ||
                                    line.contains("libc++.so"))
                        }.joinToString("\n")
                    }
                    maps.writeText(codeMappings.take(6000) + "\n")
                    val activeThreads = File("/proc/self/task").listFiles().orEmpty()
                        .take(96).mapNotNull { task ->
                            runCatching {
                                task.name + " " + File(task, "comm").readText().trim()
                            }.getOrNull()
                        }.joinToString("\n")
                    threads.writeText(activeThreads.take(5000) + "\n")
                }
                try {
                    Thread.sleep(400L)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
        }
    }

    private fun startCompileHealthMonitor(destination: File) {
        thread(name = "BunnyMonoCompileHealth", isDaemon = true) {
            val started = android.os.SystemClock.elapsedRealtime()
            while (running.get()) {
                runCatching {
                    val status = File("/proc/self/status").useLines { lines ->
                        lines.filter { it.startsWith("VmRSS:") ||
                            it.startsWith("VmHWM:") ||
                            it.startsWith("Threads:") }.joinToString("\n")
                    }
                    destination.writeText(
                        "pid=${Process.myPid()}\n" +
                        "elapsedMs=${android.os.SystemClock.elapsedRealtime() - started}\n" +
                        "managedJavaHeapBytes=${Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory()}\n" +
                        status + "\n"
                    )
                }
                try {
                    Thread.sleep(4000L)
                } catch (_: InterruptedException) {
                    break
                }
            }
        }
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
