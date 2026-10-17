package dev.bunnyloader.csharp

import java.io.File
import java.nio.file.Files
import java.security.MessageDigest
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class CSharpPrefetchReceiptTest {
    @get:Rule val temporary = TemporaryFolder()
    private val id = "68c3115f-62f4-49f2-a56b-388256781d5f"
    private fun fixture(): Triple<File, File, File> {
        val root = temporary.newFolder(id)
        File(root, "content").mkdirs()
        File(root, "content/ExampleItem.cs").writeText("public class ExampleItem {}")
        val dir = temporary.newFolder("runtime")
        File(dir, "framework-version.txt").writeText("verified-framework-A")
        val dll = File(dir, "example.dll").apply { writeText("managed-test-assembly") }
        File(dll.path + ".sha256").writeText("BUNNY_ASSEMBLY_CACHE_V1\nverified")
        return Triple(root, dir, dll)
    }
    private fun save(root: File, dir: File, dll: File): File =
        File(dir, "example.launch-ready").also {
            assertTrue(CSharpPrefetchReceipt.save(it, root, dir, id,
                CSharpPrefetchReceipt.sourceStamp(root, dir, id), dll))
            assertTrue(CSharpPrefetchReceipt.ready(it, root, dir, id, dll))
        }

    @Test fun unchangedReceiptReusesAndSameTimestampSourceEditRejects() {
        val (root, dir, dll) = fixture()
        val receipt = save(root, dir, dll)
        val source = File(root, "content/ExampleItem.cs")
        val timestamp = source.lastModified()
        source.writeText("public class ExampleEdit {}")
        source.setLastModified(timestamp)
        assertFalse(CSharpPrefetchReceipt.ready(receipt, root, dir, id, dll))
    }
    @Test fun sourceAdditionAndRemovalInvalidate() {
        val (root, dir, dll) = fixture()
        val extra = File(root, "content/Extra.cs")
        val receipt = save(root, dir, dll)
        extra.writeText("class Extra {}")
        assertFalse(CSharpPrefetchReceipt.ready(receipt, root, dir, id, dll))
        val withExtra = save(root, dir, dll)
        extra.delete()
        assertFalse(CSharpPrefetchReceipt.ready(withExtra, root, dir, id, dll))
    }
    @Test fun changedFrameworkAssemblyAndAssemblyCacheReject() {
        val (root, dir, dll) = fixture()
        var receipt = save(root, dir, dll)
        File(dir, "framework-version.txt").writeText("verified-framework-B")
        assertFalse(CSharpPrefetchReceipt.ready(receipt, root, dir, id, dll))
        receipt = save(root, dir, dll)
        dll.appendText("changed")
        assertFalse(CSharpPrefetchReceipt.ready(receipt, root, dir, id, dll))
        receipt = save(root, dir, dll)
        File(dll.path + ".sha256").appendText("changed")
        assertFalse(CSharpPrefetchReceipt.ready(receipt, root, dir, id, dll))
    }
    @Test fun editsDuringCompilationCannotPublishReceipt() {
        val (root, dir, dll) = fixture()
        val before = CSharpPrefetchReceipt.sourceStamp(root, dir, id)
        File(root, "content/ExampleItem.cs").appendText(" // edited")
        val receipt = File(dir, "example.launch-ready")
        assertFalse(CSharpPrefetchReceipt.save(receipt, root, dir, id, before, dll))
        assertFalse(receipt.exists())
    }
    @Test fun fullCompilerSourceCountSupportedAndOverLimitRejected() {
        val (root, dir, dll) = fixture()
        for (i in 1 until 4096) File(root, "content/Item$i.cs").writeText("class Item$i {}")
        assertNotNull(CSharpPrefetchReceipt.sourceStamp(root, dir, id))
        save(root, dir, dll)
        File(root, "content/OneTooMany.cs").writeText("class OneTooMany {}")
        assertNull(CSharpPrefetchReceipt.sourceStamp(root, dir, id))
    }
    @Test fun fullCompilerFileAndTotalByteLimits() {
        val (root, dir, _) = fixture()
        File(root, "content/ExampleItem.cs").delete()
        val bytes = ByteArray(1024 * 1024) { 32 }
        for (i in 0 until 32) File(root, "content/Large$i.cs").writeBytes(bytes)
        assertNotNull(CSharpPrefetchReceipt.sourceStamp(root, dir, id))
        File(root, "content/Extra.cs").writeText("x")
        assertNull(CSharpPrefetchReceipt.sourceStamp(root, dir, id))
        File(root, "content/Extra.cs").delete()
        File(root, "content/Large0.cs").appendText("x")
        assertNull(CSharpPrefetchReceipt.sourceStamp(root, dir, id))
    }
    @Test fun symlinkEscapeRejected() {
        val (root, dir, _) = fixture()
        val outside = temporary.newFile("Outside.cs").apply { writeText("class Outside {}") }
        Files.createSymbolicLink(File(root, "content/Escape.cs").toPath(), outside.toPath())
        assertNull(CSharpPrefetchReceipt.sourceStamp(root, dir, id))
    }
    @Test fun receiptV1ByteContractPreserved() {
        val (root, dir, _) = fixture()
        val digest = MessageDigest.getInstance("SHA-256")
        fun field(text: String) {
            val bytes = text.toByteArray(Charsets.UTF_8)
            digest.update(byteArrayOf(bytes.size.toByte(), (bytes.size ushr 8).toByte(),
                (bytes.size ushr 16).toByte(), (bytes.size ushr 24).toByte()))
            digest.update(bytes)
        }
        fun hex(bytes: ByteArray) = bytes.joinToString("") { "%02x".format(it.toInt() and 255) }
        field("BUNNY_PREFETCH_RECEIPT_V1")
        field("verified-framework-A")
        field(id)
        field("ExampleItem.cs")
        field(hex(MessageDigest.getInstance("SHA-256").digest(
            File(root, "content/ExampleItem.cs").readBytes())))
        assertEquals(hex(digest.digest()), CSharpPrefetchReceipt.sourceStamp(root, dir, id))
    }
}
