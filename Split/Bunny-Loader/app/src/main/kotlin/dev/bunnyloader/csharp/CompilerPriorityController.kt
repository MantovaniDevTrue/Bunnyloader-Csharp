package dev.bunnyloader.csharp

// The isolated compiler is serial. All calls are made while holding workerGate.
internal class CompilerPriorityController(private val assign: (Int, Int) -> Unit) {
    private data class Active(val modId: String, val tid: Int, val original: Int,
                              var speculative: Boolean)
    private var active: Active? = null

    fun begin(modId: String, tid: Int, original: Int, speculative: Boolean,
              officialWaiting: Boolean) {
        check(active == null) { "CompilerPriorityLeaseAlreadyActive" }
        val background = speculative && !officialWaiting
        // Publish only after the OS accepted the priority.
        assign(tid, if (background) original + 3 else original)
        active = Active(modId, tid, original, background)
    }

    fun promote(modId: String): Boolean {
        val current = active ?: return false
        if (!current.speculative || current.modId != modId) return false
        assign(current.tid, current.original)
        current.speculative = false
        return true
    }

    fun finish() {
        val current = active ?: return
        try { assign(current.tid, current.original) }
        finally { active = null }
    }
}
