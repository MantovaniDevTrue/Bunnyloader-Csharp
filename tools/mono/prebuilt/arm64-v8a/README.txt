Bunny Loader CSharp - patched Android ARM64 MonoVM 8.0.23

This artifact contains ONLY the patched open-source MonoVM library,
its SHA-256, and the .NET runtime license. There are no Terraria,
Unity, APK, keystore, game assets or secrets.

Provenance: extracted the lib/arm64-v8a/libmonosgen-2.0.so from the
previously validated Bunny Loader CSharp .72 ManagedIngress APK
(APK SHA256: 84dd410f1054690cfb4bf6a74078c0bd2c92c7ee0ab431b345e0d5c84f353feb).

Required symbols: mono_bunny_android_signal_policy_v1,
mono_jit_init, mono_thread_attach.

Source patch:
tools/mono/patch_mono_android_signals.py
Source rebuilding instructions:
tools/mono/build_patched_mono_arm64.sh
tools/mono/README.md

Install after staging the official Mono Android 8.0.23 framework
from the NuGet runtime pack. Replace ONLY
app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so.

Device tests are still required for the CSharp .95.2 minion
lifecycle fix, irrespective of this library's prior validation.
