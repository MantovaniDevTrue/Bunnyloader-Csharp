import dev.bunnyloader.game.LogcatDiagnostics;

public class LogcatDiagnosticsTest {
    private static void check(boolean value, String line) {
        if (!value) throw new AssertionError(line);
    }
    public static void main(String[] args) {
        String[] progress = {"opening-library", "resolving-symbols", "symbols-ready", "checking-assembly",
            "setting-assembly-path", "checking-system-native", "system-native-ready", "initializing-domain",
            "attaching-thread", "opening-assembly", "resolving-method", "invoking-method", "verified", "compile-verified"};
        for (String phase : progress) {
            String line = "  W/BunnyCSharp(13365): mono phase=" + phase + " detail=";
            check(!LogcatDiagnostics.isFailureHint(line) && LogcatDiagnostics.isMonoProgress(line), line);
        }
        String[] failures = {"F/libc (13365): Fatal signal 6",
            "F/libc: Abort message: mono phase=verified detail=", "JNI DETECTED ERROR IN APPLICATION",
            "Abort message: assertion failed", "E/Unity (13365): failed frame", "E/BunnyLoader: crash",
            "W/BunnyCSharp: mono phase=opening-library detail=dlopen failed",
            "W/BunnyCSharp: mono phase=compile-failed detail=", "mono phase=compile-return-invalid detail=",
            "mono phase=compiled-assembly-verifier-missing detail=", "mono phase=compile-managed-exception detail="};
        for (String line : failures) check(LogcatDiagnostics.isFailureHint(line) && !LogcatDiagnostics.isMonoProgress(line), line);
        check(!LogcatDiagnostics.isFailureHint("I/Unity: Start playing"), "normal game startup");
        System.out.println("BUNNY_LOGCAT_MONO_PROGRESS_AND_FAILURE_HINTS=PASS");
    }
}
