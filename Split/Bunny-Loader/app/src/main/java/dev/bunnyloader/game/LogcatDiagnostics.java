package dev.bunnyloader.game;

/** Report hints are evidence to inspect, not a confirmed cause of an exit. */
public final class LogcatDiagnostics {
    private LogcatDiagnostics() {}

    public static boolean isFailureHint(String line) {
        // Fatal evidence still wins when an abort message quotes a Mono phase.
        if (line.contains("F/libc") || line.contains("FORTIFY") || line.contains("Fatal signal")
            || line.contains("FatalError called") || line.contains("JNI DETECTED") || line.contains("Abort message")) return true;
        if (line.contains("mono phase=")) {
            String phase = line.substring(line.indexOf("mono phase=") + 11).split(" ", 2)[0];
            int detail = line.indexOf(" detail=");
            if (detail >= 0 && !line.substring(detail + 8).trim().isEmpty()) return true;
            return phase.contains("failed") || phase.contains("failure") || phase.contains("exception")
                || phase.contains("invalid") || phase.contains("missing") || phase.contains("unavailable")
                || phase.contains("error") || phase.contains("rejected");
        }
        return line.contains("UnsatisfiedLink") || line.contains("dlopen") || line.contains("Unable to")
            || line.contains("Failed to") || line.contains("E/Unity") || line.contains("E Unity")
            || line.contains("could not") || line.startsWith("E/BunnyLoader");
    }

    public static boolean isMonoProgress(String line) {
        return line.contains("mono phase=") && !isFailureHint(line);
    }
}
