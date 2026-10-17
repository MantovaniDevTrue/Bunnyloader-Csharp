package dev.bunnyloader.csharp

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import java.io.File
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread

internal fun compilerRequestCompleted(status: String, requestId: String): Boolean {
    if (requestId.isBlank()) return false
    val rows = status.lineSequence().toList()
    return rows.contains("requestId=$requestId") &&
        (rows.contains("state=finished") || rows.contains("state=failed"))
}

// Client-side binding: the top game process lends importance only while awaiting this request.
internal object CompilerProcessLease {
    private val activeBindings = AtomicInteger(0)
    fun start(context: Context, intent: Intent): ComponentName? {
        val app = context.applicationContext
        val requestId = UUID.randomUUID().toString()
        val modId = intent.getStringExtra("sourceModId").orEmpty()
        intent.putExtra("sourceRequestId", requestId)
        val component = app.startService(intent) ?: return null
        val handler = Handler(Looper.getMainLooper())
        val deadline = SystemClock.elapsedRealtime() + 180000L
        handler.post {
            if (activeBindings.get() >= 16) {
                Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=$modId Stage=CompilerProcessBind Reason=LeaseLimit Result=ContinueStartedService")
                return@post
            }
            val disconnected = AtomicBoolean(false)
            val connection = object : ServiceConnection {
                override fun onServiceConnected(name: ComponentName?, service: IBinder?) = Unit
                override fun onServiceDisconnected(name: ComponentName?) {
                    disconnected.set(true)
                }
                override fun onBindingDied(name: ComponentName?) { disconnected.set(true) }
                override fun onNullBinding(name: ComponentName?) { disconnected.set(true) }
            }
            val bound = runCatching {
                app.bindService(intent, connection, Context.BIND_AUTO_CREATE or Context.BIND_IMPORTANT)
            }.onFailure {
                Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=$modId Stage=CompilerProcessBind Reason=ServiceBindingFailed Result=ContinueStartedService", it)
            }.getOrDefault(false)
            if (!bound) return@post
            activeBindings.incrementAndGet()
            Log.i("BunnyCSharp", "[CSharp][Startup] Mod=$modId Stage=CompilerProcessBound Request=$requestId")
            try {
                thread(name = "BunnyCompilerLease", isDaemon = true) {
                var reason = "Deadline"
                try {
                    val status = File(app.filesDir, "bunny-csharp/source-compile-status.txt")
                    while (SystemClock.elapsedRealtime() < deadline && !disconnected.get()) {
                        val value = runCatching {
                            if (status.isFile && status.length() <= 16384L) status.readText() else ""
                        }.getOrDefault("")
                        if (compilerRequestCompleted(value, requestId)) {
                            reason = "RequestCompleted"
                            break
                        }
                        Thread.sleep(100L)
                    }
                    if (disconnected.get()) reason = "ServiceDisconnected"
                } catch (_: InterruptedException) {
                    reason = "Interrupted"
                    Thread.currentThread().interrupt()
                } finally {
                    handler.post {
                        runCatching { app.unbindService(connection) }
                            .onFailure { Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=$modId Stage=CompilerProcessUnbind Reason=BindingReleaseFailed", it) }
                        activeBindings.decrementAndGet()
                        Log.i("BunnyCSharp", "[CSharp][Startup] Mod=$modId Stage=CompilerProcessReleased Reason=$reason Request=$requestId")
                    }
                }
                }
            } catch (error: Throwable) {
                runCatching { app.unbindService(connection) }
                    .onFailure { Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=$modId Stage=CompilerProcessUnbind Reason=WatcherStartCleanupFailed", it) }
                activeBindings.decrementAndGet()
                Log.w("BunnyCSharp", "[CSharp][WARN][Startup] Mod=$modId Stage=CompilerProcessBind Reason=WatcherStartFailed Result=ContinueStartedService", error)
            }
        }
        return component
    }
}
