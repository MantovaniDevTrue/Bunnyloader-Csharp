package dev.bunnyloader.game

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.Context
import android.os.Build
import android.util.Log
import dev.bunnyloader.TAG
import java.io.File

/**
 * Trilha do boot hospedado, gravada em arquivo.
 *
 * A GameActivity roda no processo `:game`, e quando ela falha só sobra um Toast
 * que some. Sem adb no aparelho do usuário não dá para ler o logcat, então cada
 * passo vai também para um arquivo no filesDir (compartilhado entre os dois
 * processos do app) e o launcher mostra o conteúdo na tela.
 */
object BootLog {
    private const val FILE = "hosted-boot.log"

    private fun file(ctx: Context) = File(ctx.filesDir, FILE)

    /** Começa uma trilha nova (chamar no início do boot). */
    fun reset(ctx: Context) {
        runCatching { file(ctx).writeText("") }
    }

    fun add(ctx: Context, line: String) {
        Log.i(TAG, "boot: $line")
        runCatching { file(ctx).appendText(line + "\n") }
    }

    /** Registra uma falha com a causa raiz desembrulhada (reflection esconde). */
    fun fail(ctx: Context, what: String, t: Throwable?) {
        val root = t?.let { generateSequence(it) { c -> c.cause }.last() }
        val detail = root?.let { "${it.javaClass.simpleName}: ${it.message}" } ?: "sem detalhes"
        add(ctx, "FALHA: $what")
        add(ctx, "  -> $detail")
        root?.stackTrace?.take(6)?.forEach { add(ctx, "     at $it") }
    }

    /**
     * Captura exceções de QUALQUER thread do processo.
     *
     * Os try/catch da GameActivity só cobrem o onCreate; a Unity estoura depois,
     * nas threads dela, e aí só sobra "CRASH (Java)" no exit reason — sem stack.
     */
    fun installCrashHandler(ctx: Context) {
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, t ->
            runCatching {
                add(ctx, "CRASH na thread '${thread.name}'")
                var e: Throwable? = t
                var depth = 0
                while (e != null && depth++ < 3) {
                    add(ctx, "  ${e.javaClass.name}: ${e.message}")
                    e.stackTrace.take(24).forEach { add(ctx, "    at $it") }
                    e = e.cause?.also { add(ctx, "  causado por:") }
                }
            }
            previous?.uncaughtException(thread, t)
        }
    }

    private fun logcatFile(ctx: Context, generation: Int = 0) =
        File(ctx.filesDir, if (generation == 0) "hosted-logcat.log" else "hosted-logcat-prev$generation.log")

    private fun rotateLogcat(ctx: Context) {
        for (generation in 3 downTo 1) {
            val source = logcatFile(ctx, generation - 1)
            if (!source.exists()) continue
            val destination = logcatFile(ctx, generation)
            if (destination.exists()) destination.delete()
            if (!source.renameTo(destination)) {
                source.copyTo(destination, overwrite = true)
                source.delete()
            }
        }
    }

    /**
     * Captura o logcat DO PRÓPRIO processo para arquivo.
     *
     * A Unity imprime o motivo do erro no logcat imediatamente antes de abortar,
     * e um app pode ler os próprios logs sem permissão especial. Isso é bem mais
     * confiável que o tombstone, que desde o Android 12 vem em protobuf e chegou
     * truncado aqui.
     */
    fun captureLogcat(ctx: Context) {
        val out = logcatFile(ctx)
        runCatching {
            rotateLogcat(ctx)
            out.writeText("SESSION_START pid=${android.os.Process.myPid()} unixMs=${System.currentTimeMillis()}\n")
        }
        Thread {
            runCatching {
                val pid = android.os.Process.myPid()
                val p = Runtime.getRuntime().exec(
                    arrayOf("logcat", "-v", "brief", "--pid=$pid"),
                )
                p.inputStream.bufferedReader().forEachLine { line ->
                    // TAG é o primeiro da lista de propósito: sem ele o filtro
                    // descartava as linhas do nosso próprio núcleo (giveItem,
                    // cheats, mods), que são justamente as que o usuário
                    // precisa conseguir mandar quando algo não funciona.
                    val keep = line.contains(TAG) || line.contains("BunnyCSharp") ||
                        line.contains("Unity") || line.contains("IL2CPP") ||
                        line.contains("DEBUG") || line.contains("libc") ||
                        line.contains("Fatal") || line.contains("FATAL") ||
                        line.contains("Error") || line.contains("error") ||
                        line.contains("JNI") || line.contains("dlopen") ||
                        line.contains("art")
                    // Limite de tamanho: interessa a causa, não o histórico todo.
                    if (keep && out.length() < 60_000) out.appendText(line + "\n")
                }
            }
        }.apply { isDaemon = true }.start()
    }

    /**
     * O relatório do Diagnóstico (Configurações): o que um jogador que sofreu um
     * crash copia ou fotografa e manda para quem faz o Bunny Loader ou o mod.
     *
     * Na ordem de quem vai ler: o que é o aparelho e o que estava ligado, os
     * erros da última partida, e só depois o detalhe técnico do boot. Num print
     * de celular cabe o começo; por isso a causa vem antes do histórico.
     */
    fun read(ctx: Context): String =
        header(ctx) +
            "\n--- por que o jogo fechou (últimas vezes) ---\n" + exitReasons(ctx) +
            "\n--- última tentativa MonoVM ---\n" + lastMonoCheckpoint(ctx) +
            "\n--- worker MonoVM isolado ---\n" + lastIsolatedMonoWorker(ctx) +
            "\n--- último carregamento C# registrado (pode ser anterior) ---\n" + lastCSharpStartup(ctx) +
            "\n--- última compilação C# (pode ser anterior) ---\n" + lastCSharpCompilation(ctx) +
            "\n--- combate de NPCs e projéteis C# (MonoVM) ---\n" + lastCSharpEntityCombat(ctx) +
            "\n--- capturas nativas do processo do jogo ---\n" + gameCrashTraces(ctx) +
            "\n--- última partida: erros e avisos ---\n" + sessionProblems(ctx) +
            "\n--- boot do jogo ---\n" +
            (runCatching { file(ctx).readText() }.getOrDefault("").ifBlank { "(sem registro ainda)" }) +
            "\n--- logcat do processo do jogo ---\n" + lastLogcat(ctx)

    private fun lastCSharpStartup(ctx: Context): String = runCatching {
        val base = ctx.getExternalFilesDir(null)?.parentFile
            ?: return@runCatching "(sem pasta de logs)"
        val latest = File(base, "logs")
            .listFiles { entry -> entry.name.startsWith("bunny_") && entry.name.endsWith(".txt") }
            .orEmpty().maxByOrNull { it.name }
            ?: return@runCatching "(sem log da sessão)"
        val ageSeconds = ((System.currentTimeMillis() - latest.lastModified()) / 1000L)
            .coerceAtLeast(0L)
        val details = latest.useLines { lines ->
            lines.filter { line ->
                line.contains("[CSharp][Startup]") ||
                    line.contains("C# carregou sem problemas") ||
                    line.contains("inicio rapido: no mundo em")
            }.take(24).toList().joinToString("\n")
        }
        "arquivo=${latest.name} atualizadoHa=${ageSeconds}s\n" +
            details.ifBlank { "(sem medições de carregamento C# nesse log)" }
    }.getOrElse { "(erro ao ler carregamento C#: ${it.javaClass.simpleName})" }

    private fun lastCSharpEntityCombat(ctx: Context): String = runCatching {
        val file = File(ctx.filesDir, "bunny-csharp/entity-combat-status.txt")
        if (!file.isFile) return@runCatching "(sem telemetria de combate C# registrada nesta instalação)"
        val ageSeconds = ((System.currentTimeMillis() - file.lastModified()) / 1000L)
            .coerceAtLeast(0L)
        "entity-combat-status.txt registrado há ${ageSeconds}s (pode ser de sessão anterior):\\n" +
            file.readText().take(1800)
    }.getOrElse { "(erro ao ler telemetria: ${it.javaClass.simpleName})" }

    private fun lastCSharpCompilation(ctx: Context): String = buildString {
        val dir = File(ctx.filesDir, "bunny-csharp")
        val status = File(dir, "source-compile-status.txt")
        if (status.isFile) {
            val ageMs = (System.currentTimeMillis() - status.lastModified()).coerceAtLeast(0L)
            append("source-compile-status.txt (registrado há ")
                .append(ageMs / 1000L).append(" s):\n")
            append(runCatching { status.readText().take(3200) }
                .getOrElse { "(erro ao ler: ${it.javaClass.simpleName})" })
            append('\n')
        } else {
            append("source-compile-status.txt: (sem compilação registrada)\n")
        }
        val profile = File(dir, "source-compile-diagnostics.txt.profile")
        if (profile.isFile) {
            append("perfil Roslyn:\n")
            append(runCatching { profile.readText().take(1400) }
                .getOrElse { "(erro ao ler: ${it.javaClass.simpleName})" })
            append('\n')
        }
    }

    private fun lastMonoCheckpoint(ctx: Context): String = runCatching {
        val checkpoint = File(ctx.filesDir, "bunny-csharp/last-probe.txt")
        if (!checkpoint.isFile) "(nenhuma tentativa registrada)\n"
        else checkpoint.readText().take(2048).ifBlank { "(registro vazio)\n" }
    }.getOrElse { "(erro ao ler checkpoint MonoVM: ${it.javaClass.simpleName})\n" }

    private fun gameCrashTraces(ctx: Context): String = buildString {
        val dir = File(ctx.filesDir, "bunny-csharp")
        for (name in listOf("native-game-probe-mode.txt", "native-game-module-maps.txt",
            "native-game-abort-trace.txt", "native-game-abort-previous.txt")) {
            append(name).append(":\n")
            val value = runCatching { File(dir, name).readText().takeLast(8192) }
                .getOrDefault("(sem registro)")
            append(value).append('\n')
        }
    }

    private fun lastIsolatedMonoWorker(ctx: Context): String = buildString {
        val dir = File(ctx.filesDir, "bunny-csharp")
        for (name in listOf("isolated-worker-status.txt", "last-isolated-probe.txt")) {
            append(name).append(":\n")
            val value = runCatching { File(dir, name).readText().take(1600) }
                .getOrDefault("(sem registro)")
            append(value).append('\n')
        }
        val log = runCatching {
            File(dir, "isolated-worker-logcat.txt").readLines().takeLast(30).joinToString("\n")
        }.getOrDefault("(sem logcat do worker)")
        append("logcat do worker:\n").append(log).append('\n')
    }

    /** Versões, aparelho, data e os mods ligados: sem isso ninguém reproduz nada. */
    private fun header(ctx: Context): String = buildString {
        append("=== Bunny Loader: relatório ===\n")
        append("Bunny Loader v").append(dev.bunnyloader.BuildConfig.VERSION_NAME)
        append(" · Terraria ").append(BundledRuntime.VERSION_NAME)
        append(" (").append(BundledRuntime.VERSION_CODE).append(")\n")
        append("Aparelho: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
        append(" · Android ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT).append(')')
        append(" · ").append(Build.SUPPORTED_ABIS.firstOrNull() ?: "?").append('\n')
        append("Gerado em: ").append(
            java.text.SimpleDateFormat("yyyy-MM-dd HH:mm", java.util.Locale.US).format(java.util.Date()),
        ).append('\n')
        val mods = runCatching {
            val repo = dev.bunnyloader.mods.ModRepository(ctx)
            repo.list().filter { repo.isEnabled(it.uid) }
                .joinToString(", ") { "${it.name} ${it.version} (${it.id})" }
        }.getOrDefault("(não deu para ler)")
        append("Mods ligados: ").append(mods.ifBlank { "nenhum" }).append('\n')
    }

    /**
     * O log de sessão mais novo (`logs/bunny_<data>.txt`, o mesmo que o modder
     * abre): as linhas de erro e aviso, e o fim dele. Depois de um crash, o
     * mais novo é o da partida que caiu — o launcher não abre um novo.
     */
    private fun sessionProblems(ctx: Context): String = runCatching {
        val base = ctx.getExternalFilesDir(null)?.parentFile ?: return@runCatching "(sem pasta de logs)"
        val last = File(base, "logs").listFiles { f -> f.name.startsWith("bunny_") && f.name.endsWith(".txt") }
            .orEmpty().maxByOrNull { it.name } ?: return@runCatching "(nenhum log de sessão)"
        val lines = last.readLines()
        // "12:03:04.567 E texto": a letra depois da hora é o nível.
        val problems = lines.filter { it.length > 13 && (it[13] == 'E' || it[13] == 'W') }.takeLast(30)
        buildString {
            append(last.name).append('\n')
            if (problems.isEmpty()) append("(nenhum erro ou aviso)\n")
            else problems.forEach { append(it).append('\n') }
            append(">>> fim do log:\n")
            lines.takeLast(15).forEach { append(it).append('\n') }
        }
    }.getOrElse { "(erro lendo: ${it.javaClass.simpleName})" }

    /**
     * Indicios de falha primeiro, depois progresso do Mono e o resto.
     *
     * Quando a Unity aborta via JNI FatalError, a ART despeja um stack enorme
     * logo depois — e cortar pelo fim justamente esconde a mensagem, que vem
     * antes. Então destacamos as linhas de causa e só depois mostramos a cauda.
     */
    private fun lastLogcat(ctx: Context): String = buildString {
        var found = false
        for (generation in 0..3) {
            val capture = logcatFile(ctx, generation)
            val lines = runCatching { capture.readLines() }.getOrDefault(emptyList())
            if (lines.isEmpty()) continue
            found = true
            append(if (generation == 0) ">>> SESSAO ATUAL" else ">>> SESSAO ANTERIOR $generation")
            append(" | ").append(lines.first().trim()).append('\n')
            val causes = lines.filter { LogcatDiagnostics.isFailureHint(it) }.distinct().take(18)
            if (causes.isNotEmpty()) {
                append(">>> INDICIOS DE FALHA (nao confirmam a causa):\n")
                causes.forEach { append("  ").append(it.trim()).append('\n') }
            }
            val monoProgress = lines.filter { LogcatDiagnostics.isMonoProgress(it) }.takeLast(4)
            if (monoProgress.isNotEmpty()) {
                append(">>> ETAPAS MONO (contexto):\n")
                monoProgress.forEach { append("  ").append(it.trim()).append('\n') }
            }
            append(">>> FIM DO LOGCAT:\n")
            lines.filterNot { it.contains("native: #") || it.contains("runtime.cc:") }
                .takeLast(30).forEach { append(it).append('\n') }
        }
        if (!found) append("(sem captura)")
    }

    /**
     * Motivo das últimas mortes de processo do app. Crash NATIVO não passa por
     * try/catch nem aparece na trilha acima; esta API pública (Android 11+)
     * entrega o motivo e, para crash nativo, o tombstone.
     */
    private fun exitReasons(ctx: Context): String = runCatching {
        if (Build.VERSION.SDK_INT < 30) return@runCatching "(precisa Android 11+)"
        val am = ctx.getSystemService(ActivityManager::class.java)
        val list = am.getHistoricalProcessExitReasons(ctx.packageName, 0, 5)
        if (list.isEmpty()) return@runCatching "(nenhum registro)"
        list.joinToString("\n") { info ->
            buildString {
                append(info.processName).append(": ").append(reasonName(info.reason))
                append(" status=").append(info.status)
                info.description?.let { append("\n   ").append(it) }
                append(tombstone(info))
            }
        }
    }.getOrElse { "(erro lendo: ${it.javaClass.simpleName})" }

    /**
     * Para crash NATIVO o sistema guarda o tombstone (mensagem de abort +
     * backtrace) e o entrega por getTraceInputStream(). É a única forma de saber
     * por que o processo abortou sem ter adb no aparelho.
     */
    private fun tombstone(info: ApplicationExitInfo): String {
        if (info.reason != ApplicationExitInfo.REASON_CRASH_NATIVE) return ""
        return runCatching {
            val text = info.traceInputStream?.bufferedReader()?.use { it.readText() }
                ?: return "\n   (sem tombstone)"
            val frame = Regex("""^\s+#\d\d """)
            val useful = text.lineSequence()
                .filter {
                    it.contains("signal") || it.contains("Abort message") ||
                        it.contains("Cause:") || frame.containsMatchIn(it)
                }
                .take(18)
                .joinToString("\n") { "   " + it.trim() }
            if (useful.isBlank()) "\n   (tombstone sem linhas úteis)" else "\n$useful"
        }.getOrElse { "\n   (tombstone ilegível: ${it.javaClass.simpleName})" }
    }

    private fun reasonName(r: Int): String = when (r) {
        ApplicationExitInfo.REASON_CRASH -> "CRASH (Java)"
        ApplicationExitInfo.REASON_CRASH_NATIVE -> "CRASH NATIVO"
        ApplicationExitInfo.REASON_ANR -> "ANR"
        ApplicationExitInfo.REASON_LOW_MEMORY -> "SEM MEMORIA"
        ApplicationExitInfo.REASON_SIGNALED -> "SINAL"
        ApplicationExitInfo.REASON_EXIT_SELF -> "saiu sozinho"
        ApplicationExitInfo.REASON_USER_REQUESTED -> "usuario"
        ApplicationExitInfo.REASON_OTHER -> "outro"
        else -> "codigo $r"
    }
}
