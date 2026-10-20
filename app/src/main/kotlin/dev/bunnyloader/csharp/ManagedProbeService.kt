package dev.bunnyloader.csharp

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.Process
import android.util.Log
import java.io.File
import java.io.RandomAccessFile
import java.security.MessageDigest
import java.util.concurrent.LinkedBlockingDeque
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread

private object CSharpPrefetchReceipt {
    private fun hex(digest: ByteArray): String =
        digest.joinToString("") { "%02x".format(it.toInt() and 0xff) }

    private fun hash(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { stream ->
            val buffer = ByteArray(32768)
            while (true) {
                val n = stream.read(buffer)
                if (n < 0) break
                if (n > 0) digest.update(buffer, 0, n)
            }
        }
        return hex(digest.digest())
    }

    fun sourceStamp(root: File, dir: File, modId: String): String? = runCatching {
        if (!root.isDirectory || root.canonicalFile.name != modId) return@runCatching null
        val content = File(root, "content").canonicalFile
        if (!content.isDirectory) return@runCatching null
        val frameworkMarker = File(dir, "framework-version.txt")
        if (!frameworkMarker.isFile) return@runCatching null
        val digest = MessageDigest.getInstance("SHA-256")
        fun field(text: String) {
            val bytes = text.toByteArray(Charsets.UTF_8)
            digest.update(byteArrayOf((bytes.size and 0xff).toByte(),
                ((bytes.size ushr 8) and 0xff).toByte(),
                ((bytes.size ushr 16) and 0xff).toByte(),
                ((bytes.size ushr 24) and 0xff).toByte()))
            digest.update(bytes)
        }
        field("BUNNY_PREFETCH_RECEIPT_V1")
        field(frameworkMarker.readText().trim())
        field(modId)
        val sources = content.walkTopDown().filter { it.isFile && it.extension == "cs" }
            .sortedBy { it.relativeTo(content).invariantSeparatorsPath }.toList()
        if (sources.isEmpty() || sources.size > 512) return@runCatching null
        var bytesTotal = 0L
        for (file in sources) {
            val canonical = file.canonicalFile
            if (!canonical.path.startsWith(content.path + File.separator) ||
                file.length() !in 1L..262144L) return@runCatching null
            bytesTotal += file.length()
            if (bytesTotal > 8L * 1024 * 1024) return@runCatching null
            field(file.relativeTo(content).invariantSeparatorsPath)
            field(hash(file))
        }
        hex(digest.digest())
    }.getOrNull()

    fun save(receipt: File, root: File, dir: File, id: String,
             before: String?, assembly: File): Boolean {
        if (before == null || !assembly.isFile || assembly.length() <= 0) return false
        val cache = File(assembly.absolutePath + ".sha256")
        if (!cache.isFile || !cache.readText().startsWith("BUNNY_ASSEMBLY_CACHE_V1")) return false
        if (sourceStamp(root, dir, id) != before) return false
        val body = "BUNNY_PREFETCH_RECEIPT_V1\n" + before + "\n" +
            hash(assembly) + "\n" + hash(cache) + "\n"
        val temporary = File(receipt.absolutePath + ".tmp")
        return runCatching {
            temporary.writeText(body)
            if (!temporary.renameTo(receipt)) error("prefetch receipt rename failed")
            true
        }.getOrDefault(false).also { temporary.delete() }
    }

    fun ready(receipt: File, root: File, dir: File, id: String, assembly: File): Boolean =
        runCatching {
            if (!receipt.isFile || !assembly.isFile) return@runCatching false
            val rows = receipt.readLines()
            if (rows.size != 4 || rows[0] != "BUNNY_PREFETCH_RECEIPT_V1" ||
                rows[1] != sourceStamp(root, dir, id) ||
                rows[2] != hash(assembly) ||
                rows[3] != hash(File(assembly.absolutePath + ".sha256"))) return@runCatching false
            true
        }.getOrDefault(false)
}

class ManagedProbeService : Service() {
    companion object {
        @JvmStatic
        fun startCompilerRequest(context: android.content.Context, intent: Intent): android.content.ComponentName? =
            CompilerProcessLease.start(context, intent)

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


    private data class CompileWork(val startId: Int, val sourceRoot: String?, val modId: String,
                                   val precompile: Boolean, val requestId: String)
    private class ResidentRuntime {
        var assembly: String? = null
        var cryptoReady = false
        var compiled = 0
    }

    private val running = AtomicBoolean(false)
    private val workerGate = Any()
    private val compilerPriority = CompilerPriorityController { tid, value ->
        Process.setThreadPriority(tid, value)
    }
    private val pending = LinkedBlockingDeque<CompileWork>(16)
    private val healthGeneration = AtomicInteger(0)
    private var workerThread: Thread? = null
    @Volatile private var frameworkInstallState = "Unknown"

    private external fun signalHandlerSnapshot(): String
    private external fun runNative(assemblyPath: String, checkpointPath: String): String
    private external fun compileNative(
        probePath: String, compilerPath: String, sourceRoot: String,
        outputPath: String, diagnosticPath: String, checkpointPath: String
    ): String
    private external fun precompileNative(
        probePath: String, compilerPath: String, sourceRoot: String,
        outputPath: String, diagnosticPath: String, checkpointPath: String
    ): String
    private external fun verifyPrefetchedNative(
        probePath: String, compilerPath: String, sourceRoot: String,
        outputPath: String, diagnosticPath: String, checkpointPath: String
    ): String

    override fun onBind(intent: Intent?): IBinder = android.os.Binder()

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val sourceRoot = intent?.getStringExtra("sourceRoot")
        val modId = intent?.getStringExtra("sourceModId") ?: ""
        val precompile = intent?.getBooleanExtra("sourcePrecompile", false) == true
        val requestId = intent?.getStringExtra("sourceRequestId").orEmpty().take(64)
        if (sourceRoot != null &&
            (modId.isBlank() || modId.length > 128 ||
             !modId.matches(Regex("[0-9a-fA-F-]+")))) {
            runCatching {
                if (!precompile) File(filesDir, "bunny-csharp/source-compile-status.txt").apply {
                    parentFile?.mkdirs()
                    writeText("state=failed\nmodId=" + modId +
                        "\nrequestId=" + requestId + "\nerror=InvalidWorkerModIdentity\n")
                }
            }
            return START_NOT_STICKY
        }
        synchronized(workerGate) {
            if (!precompile && modId.isNotBlank()) {
                runCatching { compilerPriority.promote(modId) }
                    .onSuccess { promoted ->
                        if (promoted) Log.i("BunnyCSharp", "[CSharp][Startup] Mod=$modId Stage=PrefetchPromoted Reason=OfficialCompileWaiting Priority=Normal")
                    }
                    .onFailure { Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=$modId Stage=PrefetchPromotion Reason=ThreadPriorityChangeFailed Result=ContinueExistingCompile", it) }
            }
            if (modId.isNotBlank()) {
                pending.filter { it.precompile && it.modId == modId }
                    .forEach { pending.remove(it) }
            }
            val work = CompileWork(startId, sourceRoot, modId, precompile, requestId)
            var accepted = if (precompile) pending.offerLast(work) else pending.offerFirst(work)
            if (!accepted && !precompile) {
                val displaced = pending.peekLast()
                if (displaced?.precompile == true && pending.removeLastOccurrence(displaced))
                    accepted = pending.offerFirst(work)
            }
            if (!accepted) {
                if (!precompile) runCatching {
                    writeOfficialCompileStatus(File(filesDir, "bunny-csharp"), modId,
                        "state=failed\nmodId=" + modId +
                            "\nrequestId=" + requestId + "\nerror=CompilerQueueFull\n")
                }
                else Log.w("BunnyCSharp", "Launch precompile skipped: queue full, mod=$modId")
            } else if (workerThread == null) {
                startResidentWorker()
            }
        }
        return START_NOT_STICKY
    }

    private fun startResidentWorker() {
        running.set(true)
        workerThread = thread(name = "BunnyMonoCompiler", isDaemon = true) {
            val residency = ResidentRuntime()
            var finalStartId = 0
            try {
                while (true) {
                    val job = pending.poll(8, TimeUnit.SECONDS) ?: break
                    finalStartId = job.startId
                    executeJob(job, residency)
                    residency.compiled++
                }
            } catch (error: Throwable) {
                Log.e("BunnyCSharp", "Mono compiler worker failed", error)
            } finally {
                var hasPending = false
                synchronized(workerGate) {
                    workerThread = null
                    hasPending = pending.isNotEmpty()
                    if (hasPending) startResidentWorker()
                    else running.set(false)
                }
                if (!hasPending && finalStartId != 0) stopSelfResult(finalStartId)
            }
        }
    }

    private fun writeOfficialCompileStatus(dir: File, modId: String, value: String) {
        val status = File(dir, "source-compile-status-" + modId + ".txt")
        val pending = File(dir, status.name + ".tmp")
        dir.mkdirs()
        pending.writeText(value)
        if (!pending.renameTo(status)) {
            status.writeText(value)
            pending.delete()
        }
        File(dir, "source-compile-status.txt").writeText(value)
    }

    private fun executeJob(job: CompileWork, resident: ResidentRuntime) {
        val dir = File(filesDir, "bunny-csharp")
        dir.mkdirs()
        val checkpoint = File(dir, if (job.precompile) "last-launch-precompile.txt"
            else "last-isolated-probe.txt")
        val status = File(dir, "isolated-worker-status.txt")
        val log = File(dir, "isolated-worker-logcat.txt")
        val compileRoot = job.sourceRoot
        val compileStatus = File(dir, if (job.precompile)
            "source-launch-precompile-" + job.modId + ".txt" else "source-compile-status.txt")
        val diagnostic = File(dir, if (job.precompile)
            "source-launch-precompile-diagnostics-" + job.modId + ".txt"
            else "source-compile-diagnostics-" + job.modId + ".txt")
        val health = File(dir, if (job.precompile)
            "source-launch-precompile-health.txt" else "source-compile-health.txt")
        val nativeTrace = File(dir, "native-abort-trace.txt")
        val nativeMaps = File(dir, "native-abort-maps.txt")
        val nativeThreads = File(dir, "native-abort-threads.txt")
        val abortWatch = File(dir, "native-abort-watch.txt")
        val writeStatus: (String) -> Unit = { value ->
            if (job.precompile) compileStatus.writeText(value)
            else writeOfficialCompileStatus(dir, job.modId, value)
        }
        val workerStartedMs = android.os.SystemClock.elapsedRealtime()
        val originalPriority = Process.getThreadPriority(Process.myTid())
        val reused = resident.assembly != null
        val currentHealth = healthGeneration.incrementAndGet()
        try {
            synchronized(workerGate) {
                val officialWaiting = pending.any { !it.precompile && it.modId == job.modId }
                compilerPriority.begin(job.modId, Process.myTid(), originalPriority,
                    job.precompile, officialWaiting)
                if (job.precompile && officialWaiting)
                    Log.i("BunnyCSharp", "[CSharp][Startup] Mod=${job.modId} Stage=PrefetchPromoted Reason=OfficialAlreadyQueued Priority=Normal")
            }
            checkpoint.delete()
            if (compileRoot == null) {
                log.delete()
                status.writeText("state=starting\npid=" + Process.myPid() + "\n")
                startLogCapture(log)
            } else {
                diagnostic.delete()
                File(diagnostic.absolutePath + ".profile").delete()
                if (!job.precompile) {
                    for (name in listOf(
                        "source-runtime-values.txt", "source-ai-turns.txt",
                        "source-spark-turns.txt", "source-projectile-turns.txt",
                        "source-homing-policy.txt", "source-onhit-policy.txt",
                        "source-onkill-policy.txt"
                    )) {
                        File(dir, name).delete()
                        File(dir, name + ".tmp").delete()
                    }
                }
                health.delete()
                if (!reused && !job.precompile) {
                    nativeTrace.delete()
                    nativeMaps.delete()
                    nativeThreads.delete()
                    abortWatch.delete()
                }
                writeStatus("state=preparing-framework\nmodId=" +
                    job.modId + "\nrequestId=" + job.requestId + "\npid=" + Process.myPid() + "\n")
                startCompileHealthMonitor(health, currentHealth)
            }
            System.loadLibrary("bunnymonoprobe")
            if (compileRoot != null && !resident.cryptoReady) {
                writeStatus("state=preparing-crypto-jni\nmodId=" +
                    job.modId + "\nrequestId=" + job.requestId + "\npid=" + Process.myPid() + "\n")
                Class.forName("net.dot.android.crypto.DotnetProxyTrustManager")
                System.loadLibrary("System.Security.Cryptography.Native.Android")
                resident.cryptoReady = true
            }
            if (resident.assembly == null) resident.assembly = installManagedFramework(dir)
            else frameworkInstallState = "ResidentVerified"
            val assembly = checkNotNull(resident.assembly)
            val frameworkSetupMs = android.os.SystemClock.elapsedRealtime() - workerStartedMs
            if (compileRoot == null) {
                status.writeText("state=calling-mono\npid=" + Process.myPid() + "\n")
                val result = runNative(assembly, checkpoint.absolutePath)
                status.writeText("state=finished\npid=" + Process.myPid() + "\n" + result + "\n")
                return
            }
            val compiler = File(dir, "BunnySourceCompiler.dll")
            val outputDir = File(dir, "compiled")
            outputDir.mkdirs()
            val output = File(outputDir, job.modId + ".dll")
            val receipt = File(outputDir, job.modId + ".launch-ready")
            if (job.precompile &&
                CSharpPrefetchReceipt.ready(receipt, File(compileRoot), dir, job.modId, output)) {
                writeStatus(
                    "state=finished\nmodId=" + job.modId +
                    "\npid=" + Process.myPid() +
                    "\nphase=compile-prefetched-cache-hit\nassemblyReady=1" +
                    "\nprefetchCacheHit=1\nprecompileOnly=1" +
                    "\nframeworkCache=" + frameworkInstallState + "\n"
                )
                Log.i("BunnyCSharp", "[CSharp][Startup] Stage=LauncherPrecompileCacheHit Mod=" + job.modId)
                return
            }
            val precompileStamp = if (job.precompile) {
                receipt.delete()
                CSharpPrefetchReceipt.sourceStamp(File(compileRoot), dir, job.modId)
            } else null
            val canReuse = !job.precompile &&
                CSharpPrefetchReceipt.ready(receipt, File(compileRoot), dir, job.modId, output)
            writeStatus("state=compiling\nmodId=" + job.modId +
                "\npid=" + Process.myPid() + "\nwatchers=memory-monitor\n")
            val compilerStarted = android.os.SystemClock.elapsedRealtime()
            val compilerCpuStarted = Process.getElapsedCpuTime()
            val result = if (job.precompile) precompileNative(
                assembly, compiler.absolutePath, compileRoot,
                output.absolutePath, diagnostic.absolutePath, checkpoint.absolutePath
            ) else if (canReuse) verifyPrefetchedNative(
                assembly, compiler.absolutePath, compileRoot,
                output.absolutePath, diagnostic.absolutePath, checkpoint.absolutePath
            ) else compileNative(
                assembly, compiler.absolutePath, compileRoot,
                output.absolutePath, diagnostic.absolutePath, checkpoint.absolutePath
            )
            val managedCompileMs = android.os.SystemClock.elapsedRealtime() - compilerStarted
            val managedProcessCpuMs = (Process.getElapsedCpuTime() - compilerCpuStarted).coerceAtLeast(0L)
            val workerTotalMs = android.os.SystemClock.elapsedRealtime() - workerStartedMs
            val passed = result.contains(if (job.precompile)
                    "phase=compile-prefetched" else "phase=compile-verified") &&
                (result.contains("result=1\n") || result.contains("result=2\n"))
            if (job.precompile && passed) {
                CSharpPrefetchReceipt.save(receipt, File(compileRoot), dir, job.modId,
                    precompileStamp, output)
            } else if (!job.precompile) {
                receipt.delete()
            }
            val savedProfile = File(dir, "source-roslyn-profile-" + job.modId + ".txt")
            val profileFile = File(diagnostic.absolutePath + ".profile")
            if (!canReuse && passed && profileFile.isFile) {
                runCatching { savedProfile.writeText(profileFile.readText().take(1536) +
                    " nativeWallMs=$managedCompileMs workerProcessCpuMs=$managedProcessCpuMs") }
                    .onFailure { Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=${job.modId} Stage=SaveRoslynProfile Reason=ProfileWriteFailed", it) }
            }
            val roslynProfile = runCatching {
                (if (canReuse) savedProfile else profileFile)
                    .takeIf { it.isFile }?.readText()?.trim()?.replace('\n', ' ')?.take(1024)
            }.getOrNull() ?: "Unavailable"
            Log.i("BunnyCSharp", "[CSharp][Startup] Mod=${job.modId} Stage=RoslynProfile PrefetchReused=${if (canReuse) 1 else 0} Detail=$roslynProfile")
            writeStatus(
                "state=" + (if (passed) "finished" else "failed") + "\n" +
                "modId=" + job.modId + "\nrequestId=" + job.requestId + "\npid=" + Process.myPid() + "\n" + result + "\n" +
                "assemblyReady=" + (if (passed && output.isFile) 1 else 0) + "\n" +
                "frameworkCache=" + frameworkInstallState + "\n" +
                "frameworkSetupMs=" + frameworkSetupMs + "\n" +
                "managedCompileMs=" + managedCompileMs + "\n" +
                "managedProcessCpuMs=" + managedProcessCpuMs + "\n" +
                "workerTotalMs=" + workerTotalMs + "\n" +
                "precompileOnly=" + (if (job.precompile) 1 else 0) + "\n" +
                "prefetchReused=" + (if (canReuse) 1 else 0) + "\n" +
                "residentReuse=" + (if (reused) 1 else 0) + "\n" +
                "residentBatchIndex=" + resident.compiled + "\n" +
                "roslynProfile=" + roslynProfile + "\n"
            )
        } catch (error: Throwable) {
            val detail = (error.javaClass.simpleName + ": " +
                (error.message ?: "")).take(512)
            runCatching {
                val failure = "state=failed\nmodId=" + job.modId +
                    "\nrequestId=" + job.requestId + "\npid=" + Process.myPid() + "\nerror=" + detail + "\n"
                if (compileRoot == null) status.writeText(failure) else writeStatus(failure)
            }
            Log.e("BunnyCSharp", "Mono worker failed", error)
        } finally {
            synchronized(workerGate) {
                runCatching { compilerPriority.finish() }
                    .onFailure { Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=${job.modId} Stage=RestoreCompilerPriority Reason=ThreadPriorityChangeFailed", it) }
            }
            healthGeneration.compareAndSet(currentHealth, currentHealth + 1)
        }
    }

    private fun installManagedFramework(dir: File): String {
        val result = ManagedFrameworkAssets.ensure(this, dir, verifyCached = true)
        frameworkInstallState = result.state
        Log.i("BunnyCSharp", "framework " + result.state +
            ": installed=" + result.installed + " retained=" + result.retained)
        return result.assemblyPath
    }

    private fun startAbortHandlerWatch(destination: File) {
        thread(name = "BunnyMonoAbortWatch", isDaemon = true) {
            val started = android.os.SystemClock.elapsedRealtime()
            var previous = ""
            var transitions = 0
            repeat(200) {
                if (!running.get()) return@thread
                val state = runCatching { signalHandlerSnapshot() }
                    .getOrElse { "snapshot-error=" + it.javaClass.simpleName }
                if (state != previous) {
                    previous = state
                    if (transitions < 20) {
                        runCatching {
                            destination.appendText(
                                "elapsedMs=" +
                                    (android.os.SystemClock.elapsedRealtime() - started) +
                                    " " + state.take(450) + "\n"
                            )
                        }
                        transitions++
                    }
                }
                try {
                    Thread.sleep(30L)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
        }
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

    private fun startCompileHealthMonitor(destination: File, generation: Int) {
        thread(name = "BunnyMonoCompileHealth", isDaemon = true) {
            val started = android.os.SystemClock.elapsedRealtime()
            while (running.get() && healthGeneration.get() == generation) {
                runCatching {
                    val status = File("/proc/self/status").useLines { lines ->
                        lines.filter { it.startsWith("VmRSS:") ||
                            it.startsWith("VmHWM:") ||
                            it.startsWith("VmSize:") ||
                            it.startsWith("VmSwap:") ||
                            it.startsWith("Threads:") }.joinToString("\n")
                    }
                    val snapshot =
                        "pid=${Process.myPid()}\n" +
                        "elapsedMs=${android.os.SystemClock.elapsedRealtime() - started}\n" +
                        "managedJavaHeapBytes=${Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory()}\n" +
                        status + "\n"
                    val pending = File(destination.parentFile, destination.name + ".tmp")
                    pending.writeText(snapshot)
                    if (!pending.renameTo(destination)) {
                        destination.writeText(snapshot)
                        pending.delete()
                    }
                }
                try {
                    Thread.sleep(1500L)
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

internal object ManagedFrameworkAssets {
    data class Result(val assemblyPath: String, val state: String,
                      val installed: Int, val retained: Int)

    fun <T> withLock(dir: File, action: () -> T): T {
        require(dir.isDirectory || dir.mkdirs()) { "CSharp framework directory unavailable" }
        return RandomAccessFile(File(dir, "framework.install.lock"), "rw").use { file ->
            file.channel.lock().use { action() }
        }
    }

    fun ensure(context: android.content.Context, dir: File, verifyCached: Boolean): Result =
        withLock(dir) { ensureUnlocked(context, dir, verifyCached) }

    private fun sha256(bytes: ByteArray): String =
        java.security.MessageDigest.getInstance("SHA-256")
            .digest(bytes).joinToString("") { "%02x".format(it.toInt() and 0xff) }

    private fun sha256(file: File): String {
        val digest = java.security.MessageDigest.getInstance("SHA-256")
        val buffer = ByteArray(65536)
        file.inputStream().use { input ->
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                if (count > 0) digest.update(buffer, 0, count)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun ensureUnlocked(context: android.content.Context, dir: File, verifyCached: Boolean): Result {
        val assets = context.assets
        val names = (assets.list("managed").orEmpty()
            .filter { it.endsWith(".dll") } +
            assets.list("managed/compiler-references").orEmpty()
                .filter { it.endsWith(".dll") }.map { "compiler-references/$it" }).sorted()
        for (required in listOf("System.Private.CoreLib.dll", "System.Runtime.dll",
            "BunnyManagedProbe.dll", "BunnySourceCompiler.dll", "BunnySourceSdk.dll",
            "Microsoft.CodeAnalysis.dll", "Microsoft.CodeAnalysis.CSharp.dll"))
            require(required in names) { "CSharp framework missing: $required" }
        val manifest = assets.open("managed/framework.sha256").bufferedReader().use {
            it.readText()
        }
        val expected = manifest.lineSequence().filter { it.isNotBlank() }.associate { line ->
            val separator = line.indexOf('\t')
            require(separator > 0) { "Invalid CSharp framework manifest" }
            val name = line.substring(0, separator)
            val hash = line.substring(separator + 1).trim()
            require(hash.length == 64 && hash.all { it in '0'..'9' || it in 'a'..'f' })
            name to hash
        }
        require(expected.size == names.size && expected.keys.containsAll(names)) {
            "CSharp framework APK manifest mismatch"
        }
        val marker = File(dir, "framework-version.txt")
        val fingerprint = sha256(manifest.toByteArray(Charsets.UTF_8))
        val previousFingerprint = runCatching {
            if (marker.isFile) marker.readText().trim() else ""
        }.getOrDefault("")
        val markerMatches = previousFingerprint == fingerprint ||
            previousFingerprint.endsWith(":" + fingerprint)
        val path = File(dir, "BunnyManagedProbe.dll").absolutePath
        val approval = File(dir, "compiler-references.approved")
        val hasPackagedReferences = names.any { it.startsWith("compiler-references/") }
        if (!hasPackagedReferences) {
            approval.delete()
            val stale = File(dir, "compiler-references")
            if (stale.isDirectory) {
                if (stale.deleteRecursively())
                    Log.i("BunnyCSharp", "[CSharp][Startup] Stage=DiscardStaleCompactReferences Result=RuntimeReferences")
                else
                    Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Stage=DiscardStaleCompactReferences Result=Ignored")
            }
        }
        if (markerMatches && !verifyCached) return Result(path, "MarkerReady", 0, names.size)
        val valid = HashSet<String>(names.size)
        for (name in names) {
            val current = File(dir, name)
            if (current.isFile && current.length() > 0L &&
                sha256(current) == expected[name]) valid.add(name)
        }
        if (valid.size == names.size) {
            if (previousFingerprint != fingerprint) marker.writeText(fingerprint)
            verifySdk(dir)
            if (hasPackagedReferences) approval.writeText(fingerprint)
            return Result(path, "HitVerified", 0, names.size)
        }
        marker.delete()
        var installed = 0
        for (name in names) {
            if (name in valid) continue
            val target = File(dir, name)
            val temporary = File(dir, "$name.framework.tmp")
            temporary.parentFile?.mkdirs()
            try {
                assets.open("managed/$name").use { source ->
                    temporary.outputStream().use { output -> source.copyTo(output) }
                }
                require(temporary.isFile && temporary.length() > 0L &&
                    sha256(temporary) == expected[name]) {
                    "Assembly do APK falhou verificacao SHA-256: $name"
                }
                require(temporary.renameTo(target)) {
                    "Assembly CSharp nao pode ser instalado atomicamente: $name"
                }
                installed++
            } finally {
                temporary.delete()
            }
        }
        verifySdk(dir)
        marker.writeText(fingerprint)
        if (hasPackagedReferences) approval.writeText(fingerprint)
        return Result(path, "InstalledVerified", installed, names.size - installed)
    }

    private fun verifySdk(dir: File) {
        val sdk = File(dir, "BunnySourceSdk.dll")
        require(sdk.isFile && sdk.readBytes().toString(Charsets.ISO_8859_1)
            .contains("MoveSpeedPercent")) {
            "BunnySourceSdk.dll nao possui MoveSpeedPercent; verifique a versao do APK"
        }
    }
}
