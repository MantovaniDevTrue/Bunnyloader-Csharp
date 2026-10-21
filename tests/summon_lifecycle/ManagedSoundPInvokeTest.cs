using System;
using System.IO;
using System.Runtime.InteropServices;
using BunnyLoader.Source;

internal static class ManagedSoundPInvokeTest
{
    [DllImport("bunny", EntryPoint = "bunny_sound_test_mount", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Mount([MarshalAs(UnmanagedType.LPUTF8Str)] string root);
    [DllImport("bunny", EntryPoint = "bunny_sound_test_pump", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Pump();
    [DllImport("bunny", EntryPoint = "bunny_sound_test_world_exit", CallingConvention = CallingConvention.Cdecl)]
    private static extern void WorldExit();
    [DllImport("bunny", EntryPoint = "bunny_sound_test_stops", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Stops();
    [DllImport("bunny", EntryPoint = "bunny_sound_test_loads", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Loads();
    [DllImport("bunny", EntryPoint = "bunny_sound_test_x", CallingConvention = CallingConvention.Cdecl)]
    private static extern int X();
    [DllImport("bunny", EntryPoint = "bunny_sound_test_y", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Y();

    private static void Check(bool condition, string name)
    {
        if (!condition) throw new Exception("SoundLoader PInvoke regression: " + name);
    }

    private static void Main()
    {
        string root = Path.Combine(Path.GetTempPath(), "bunny-sound-pinvoke-" + Guid.NewGuid().ToString("N"));
        string sounds = Path.Combine(root, "content", "Assets", "Sounds");
        Directory.CreateDirectory(sounds);
        File.WriteAllBytes(Path.Combine(sounds, "Example.ogg"), new byte[] { 1, 2, 3 });
        try
        {
            Mount(root);
            var style = new BunnySoundStyle("Assets/Sounds/Example.ogg",
                volume: 0.7f, pitchVariance: 0.12f, maxInstances: 3,
                limitBehavior: BunnySoundLimitBehavior.IgnoreNew);
            var clip = BunnySounds.Register("sound-pinvoke-fixture", style);
            Check(clip.State == 0 && clip.DurationMilliseconds == 0 && Loads() == 0, "deferred-register");
            var pending = clip.Play(320, 160);
            Check(pending.Request > 0 && pending.Token == 0, "deferred-play");
            Check(clip.FindActive() == 0, "no-sound-before-pump");

            Pump();
            Check(clip.State == 1 && clip.DurationMilliseconds == 550, "real-sdk-native-state-and-duration");
            Check(pending.Token > 0 && pending.Token == clip.FindActive(), "real-sdk-native-play");
            Check(X() == 320 && Y() == 160 && Loads() == 1, "position-and-cached-asset");

            var same = BunnySounds.Register("sound-pinvoke-fixture", style);
            Check(clip.Handle == same.Handle && Loads() == 1, "idempotence");
            int stops = Stops();
            Check(pending.Stop(), "stop-accepted");
            Check(pending.Token == -1, "stop-invalidates-request");
            Pump();
            Check(Stops() == stops + 1, "native-stop");

            var cancelled = clip.Play();
            Check(cancelled.Stop(), "cancel-pending");
            Pump();
            Check(cancelled.Token == -1 && Stops() == stops + 1, "no-audio-after-cancel");
            var another = clip.Play();
            Pump();
            Check(another.Token > 0 && X() == -1 && Y() == -1, "interface-sound");
            WorldExit();
            Check(another.Token == -1 && clip.State == 1, "world-exit-invalidation");

            bool invalidName = false, invalidPath = false;
            try { BunnySounds.Register("unowned-fixture", style); }
            catch (InvalidOperationException) { invalidName = true; }
            try { _ = new BunnySoundStyle("../missing.ogg"); }
            catch (ArgumentException) { invalidPath = true; }
            Check(invalidName && invalidPath, "owner-and-path-validation");

            BunnySounds.UnloadMod("sound-pinvoke-fixture");
            Check(clip.State == -2 && !another.Stop(), "unloaded-source-is-stale");
            Pump();
            var remounted = BunnySounds.Register("sound-pinvoke-fixture", style);
            Check(remounted.Handle != clip.Handle, "new-generation-after-unload");
            Pump();
            Check(remounted.State == 1 && Loads() == 1, "native-audio-cache-reuse");
            BunnySounds.UnloadMod("sound-pinvoke-fixture");
            Pump();
            Console.WriteLine("CSHARP_SOUNDLOADER_MANAGED_DLL_TO_NATIVE_PINVOKE_TO_ANDROID_AUDIO_MOCK_PASS");
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }
    }
}
