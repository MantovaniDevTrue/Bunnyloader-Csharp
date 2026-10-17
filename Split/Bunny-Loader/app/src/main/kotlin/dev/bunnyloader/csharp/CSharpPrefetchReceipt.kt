package dev.bunnyloader.csharp

import java.io.File
import java.security.MessageDigest

internal object CSharpPrefetchReceipt {
    private fun hex(digest: ByteArray): String {
        val digits = "0123456789abcdef"
        return CharArray(digest.size * 2).also { result ->
            for (i in digest.indices) {
                val value = digest[i].toInt() and 0xff
                result[i * 2] = digits[value ushr 4]
                result[i * 2 + 1] = digits[value and 15]
            }
        }.concatToString()
    }

    private fun hash(file: File, buffer: ByteArray = ByteArray(32768)): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { stream ->
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
        val buffer = ByteArray(32768)
        val sources = content.walkTopDown().filter { it.isFile && it.extension == "cs" }
            .take(4097).sortedBy { it.relativeTo(content).invariantSeparatorsPath }.toList()
        if (sources.isEmpty() || sources.size > 4096) return@runCatching null
        var bytesTotal = 0L
        for (file in sources) {
            val canonical = file.canonicalFile
            if (!canonical.path.startsWith(content.path + File.separator) ||
                file.length() !in 1L..1048576L) return@runCatching null
            bytesTotal += file.length()
            if (bytesTotal > 32L * 1024 * 1024) return@runCatching null
            field(file.relativeTo(content).invariantSeparatorsPath)
            field(hash(file, buffer))
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

