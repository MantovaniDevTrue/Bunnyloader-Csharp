package dev.bunnyloader.game

import android.media.AudioAttributes
import android.media.MediaMetadataRetriever
import android.media.MediaPlayer
import android.media.SoundPool
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import android.util.Log
import java.util.concurrent.ConcurrentHashMap

object ModAudio {
    private const val TAG = "BunnyLoader"

    private val loaded: MutableSet<Int> = ConcurrentHashMap.newKeySet()
    private val failed: MutableSet<Int> = ConcurrentHashMap.newKeySet()
    private val durations = ConcurrentHashMap<Int, Int>()
    private val paths = ConcurrentHashMap<Int, String>()

    private val effects by lazy {
        SfxQueue(object : SfxQueue.Backend {
            override fun play(id: Int, left: Float, right: Float, rate: Float) = pool.play(id, left, right, 1, 0, rate)
            override fun stop(stream: Int) = pool.stop(stream)
            override fun pause() { if (poolCreated) pool.autoPause() }
            override fun resume() { if (poolCreated) pool.autoResume() }
        })
    }

    @Volatile private var poolCreated = false

    private val pool: SoundPool by lazy {
        poolCreated = true
        SoundPool.Builder()
            .setMaxStreams(32)
            .setAudioAttributes(attributes(AudioAttributes.CONTENT_TYPE_SONIFICATION))
            .build()
            .apply {
                setOnLoadCompleteListener { _, id, status ->
                    if (status == 0) loaded += id else {
                        failed += id
                        Log.e(TAG, "[Audio][ERROR] AssetId=$id Path=${paths[id] ?: "<path not yet registered>"} Stage=Decode ExpectedStatus=0 ActualStatus=$status Reason=AndroidSoundPoolLoadFailed Result=SoundUnavailable")
                    }
                }
            }
    }

    private fun attributes(content: Int): AudioAttributes =
        AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(content)
            .build()

    @JvmStatic
    fun load(path: String): Int {
        val id = pool.load(path, 1)
        if (id > 0) {
            paths[id] = path
            durations[id] = durationOf(path)
        } else Log.e(TAG, "[Audio][ERROR] Path=$path Stage=Load Expected=PositiveSoundId Actual=$id Reason=AndroidSoundPoolLoadRejected Result=SoundUnavailable")
        return id
    }

    @JvmStatic
    fun state(id: Int): Int = when (id) {
        in loaded -> 1
        in failed -> -1
        else -> 0
    }

    @JvmStatic
    fun duration(id: Int): Int = durations[id] ?: 0

    @JvmStatic
    fun play(id: Int, left: Float, right: Float, rate: Float): Int =
        if (id in loaded) pool.play(id, left, right, 1, 0, rate) else 0

    @JvmStatic
    fun stop(stream: Int) {
        if (poolCreated) pool.stop(stream)
    }

    /** Accept a short effect without waiting for SoundPool. Handle != native stream. */
    @JvmStatic
    fun enqueue(group: Int, id: Int, left: Float, right: Float, rate: Float, max: Int, ignore: Boolean): Int {
        if (id !in loaded) return 0
        return effects.enqueue(group, id, left, right, rate, duration(id), max, ignore)
    }

    @JvmStatic
    fun cancel(handle: Int) = effects.stop(handle)

    @JvmStatic
    fun playbackState(handle: Int): Int = effects.state(handle)

    @JvmStatic
    fun activeSound(group: Int): Int = effects.active(group)

    @JvmStatic
    fun queueStats(): String = effects.stats()

    private fun durationOf(path: String): Int {
        val r = MediaMetadataRetriever()
        return try {
            r.setDataSource(path)
            r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toIntOrNull() ?: 0
        } catch (_: Exception) {
            0
        } finally {
            r.release()
        }
    }

    private class Track(val path: String) {
        var player: MediaPlayer? = null
        var prepared = false
        var failed = false
        var volume = 0f
        var pausedByApp = false
        var generation = 0
        var rampFrom = 0f
        var rampTo = 0f
        var rampStart = 0L
        var rampDuration = 1L
        var prepareQueued = false
    }

    private val tracks = ConcurrentHashMap<Int, Track>()
    @Volatile private var nextTrack = 1

    private val musicThread: HandlerThread by lazy {
        HandlerThread("BunnyModMusic").apply { start() }
    }

    private val musicHandler: Handler by lazy {
        Handler(musicThread.looper)
    }

    @JvmStatic
    @Synchronized
    fun musicRegister(path: String): Int {
        val id = nextTrack++
        tracks[id] = Track(path)
        queuePrepare(id)
        return id
    }

    @JvmStatic
    @Synchronized
    fun musicVolume(id: Int, volume: Float) {
        val t = tracks[id] ?: return
        if (t.failed) return
        t.generation++
        t.volume = volume.coerceIn(0f, 1f)
        applyTrackVolume(id, t)
    }

    @JvmStatic
    @Synchronized
    fun musicRamp(id: Int, from: Float, to: Float, delayMs: Int, durationMs: Int) {
        val t = tracks[id] ?: return
        if (t.failed) return

        val start = from.coerceIn(0f, 1f)
        val end = to.coerceIn(0f, 1f)
        val delay = delayMs.coerceAtLeast(0).toLong()
        t.generation++
        val generation = t.generation
        t.rampFrom = start
        t.rampTo = end
        t.rampStart = SystemClock.uptimeMillis() + delay
        t.rampDuration = durationMs.coerceAtLeast(1).toLong()
        t.volume = start

        if (t.player == null && (start > 0f || end > 0f)) queuePrepare(id)
        if (t.prepared) applyTrackVolume(id, t)

        musicHandler.postDelayed({ runRamp(id, generation) }, delay)
    }

    @JvmStatic
    @Synchronized
    fun musicState(id: Int): Int {
        val t = tracks[id] ?: return -1
        return when {
            t.failed -> -1
            t.player == null -> 0
            !t.prepared -> 1
            t.player!!.isPlaying -> 2
            else -> 0
        }
    }

    private fun queuePrepare(id: Int) {
        val t = tracks[id] ?: return
        if (t.failed || t.prepared || t.player != null || t.prepareQueued) return
        t.prepareQueued = true
        musicHandler.post {
            synchronized(this) {
                val current = tracks[id] ?: return@synchronized
                if (current.failed || current.prepared || current.player != null) {
                    current.prepareQueued = false
                    return@synchronized
                }
                current.prepareQueued = false
                prepare(id, current)
            }
        }
    }

    private fun prepare(id: Int, t: Track): MediaPlayer {
        t.prepareQueued = false
        val p = MediaPlayer()
        t.player = p
        try {
            p.setAudioAttributes(attributes(AudioAttributes.CONTENT_TYPE_MUSIC))
            p.setDataSource(t.path)
            p.isLooping = true
            p.setOnPreparedListener { onPrepared(id) }
            p.setOnErrorListener { _, what, extra ->
                Log.e(TAG, "musica de mod ${t.path}: erro $what/$extra")
                synchronized(this) { t.failed = true }
                true
            }
            p.prepareAsync()
        } catch (e: Exception) {
            Log.e(TAG, "musica de mod ${t.path}: ${e.message}")
            t.failed = true
        }
        return p
    }

    @Synchronized
    private fun onPrepared(id: Int) {
        val t = tracks[id] ?: return
        t.prepared = true
        applyTrackVolume(id, t)
    }

    @Synchronized
    private fun runRamp(id: Int, generation: Int) {
        val t = tracks[id] ?: return
        if (t.failed || t.generation != generation) return

        val now = SystemClock.uptimeMillis()
        if (now < t.rampStart) {
            musicHandler.postDelayed({ runRamp(id, generation) }, t.rampStart - now)
            return
        }

        val elapsed = now - t.rampStart
        val progress = (elapsed.toFloat() / t.rampDuration.toFloat()).coerceIn(0f, 1f)
        t.volume = t.rampFrom + (t.rampTo - t.rampFrom) * progress
        applyTrackVolume(id, t)

        if (progress >= 1f) return
        musicHandler.postDelayed({ runRamp(id, generation) }, 16L)
    }

    private fun applyTrackVolume(id: Int, t: Track) {
        if (t.failed) return
        val p = t.player ?: run {
            if (t.volume > 0f) queuePrepare(id)
            return
        }
        if (!t.prepared) return

        if (t.volume > 0f) {
            p.setVolume(t.volume, t.volume)
            if (!p.isPlaying && !t.pausedByApp) p.start()
        } else {
            if (p.isPlaying) p.pause()
            p.seekTo(0)
        }
    }

    @JvmStatic
    @Synchronized
    fun pauseAll() {
        effects.pause()
        for (t in tracks.values) {
            val p = t.player ?: continue
            if (t.prepared && p.isPlaying) {
                p.pause()
                t.pausedByApp = true
            }
        }
    }

    @JvmStatic
    @Synchronized
    fun resumeAll() {
        effects.resume()
        for (t in tracks.values) {
            if (!t.pausedByApp) continue
            t.pausedByApp = false
            if (t.volume > 0f) t.player?.start()
        }
    }
}
