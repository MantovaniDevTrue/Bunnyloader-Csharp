package dev.bunnyloader.mods

object ScriptRuntimeModes {
    data class Selection(val csharpCount: Int, val javaScriptCount: Int) {
        val mixed: Boolean get() = csharpCount > 0 && javaScriptCount > 0
        fun disableQuickJs(requested: Boolean): Boolean =
            requested && csharpCount > 0 && javaScriptCount == 0
    }

    fun fromSpecs(specs: Iterable<String>): Selection {
        var csharp = 0
        var javascript = 0
        for (spec in specs) {
            val entry = spec.substringAfter('=', "main.js")
            if (entry.startsWith("@")) continue
            if (entry.endsWith(".cs")) csharp++ else javascript++
        }
        return Selection(csharp, javascript)
    }
}
