package dev.bunnyloader.csharp

import org.junit.Assert.*
import org.junit.Test

class CompilerPriorityControllerTest {
    @Test fun officialRequestPromotesSameRunningCompileOnceAndRestores() {
        val calls = mutableListOf<Pair<Int, Int>>()
        val controller = CompilerPriorityController { tid, value -> calls.add(tid to value) }
        controller.begin("example", 17, 0, true, false)
        assertEquals(listOf(17 to 3), calls)
        assertFalse(controller.promote("other"))
        assertTrue(controller.promote("example"))
        assertFalse(controller.promote("example"))
        controller.finish()
        assertEquals(listOf(17 to 3, 17 to 0, 17 to 0), calls)
        assertFalse(controller.promote("example"))
    }
    @Test fun requestArrivingBeforeWorkerPublicationNeverLowersPriority() {
        val calls = mutableListOf<Int>()
        val controller = CompilerPriorityController { _, value -> calls.add(value) }
        controller.begin("example", 18, 0, true, true)
        assertEquals(listOf(0), calls)
        assertFalse(controller.promote("example"))
        controller.finish()
    }
    @Test fun followingSpeculativeModGetsItsOwnBackgroundLease() {
        val calls = mutableListOf<Int>()
        val controller = CompilerPriorityController { _, value -> calls.add(value) }
        controller.begin("first", 19, 0, true, false)
        controller.promote("first")
        controller.finish()
        controller.begin("second", 19, 0, true, false)
        assertEquals(3, calls.last())
        controller.finish()
    }
    @Test fun failedPromotionCanRetryWithoutPretendingItSucceeded() {
        var rejected = false
        val controller = CompilerPriorityController { _, _ -> if (rejected) error("OS rejected") }
        controller.begin("example", 20, 0, true, false)
        rejected = true
        try { controller.promote("example"); fail("Expected exception") }
        catch (_: IllegalStateException) {}
        rejected = false
        assertTrue(controller.promote("example"))
        controller.finish()
    }
}
